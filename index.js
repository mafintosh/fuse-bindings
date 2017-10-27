var fuse = require('node-gyp-build')(__dirname)
var fs = require('fs')
var os = require('os')
var xtend = require('xtend')
var path = require('path')

var noop = function () {}
var call = function (cb) { cb() }

var IS_OSX = os.platform() === 'darwin'
var OSX_FOLDER_ICON = '/System/Library/CoreServices/CoreTypes.bundle/Contents/Resources/GenericFolderIcon.icns'
var HAS_FOLDER_ICON = IS_OSX && fs.existsSync(OSX_FOLDER_ICON)

var FuseBuffer = function () {
  this.length = 0
  this.parent = undefined
}

FuseBuffer.prototype = Buffer.prototype

fuse.setBuffer(FuseBuffer)
fuse.setCallback(function (index, callback) {
  return callback.bind(null, index)
})

exports.context = function () {
  var ctx = {}
  fuse.populateContext(ctx)
  return ctx
}

exports.mount = function (mnt, ops, opts, cb) {
  if (typeof opts === 'function') return exports.mount(mnt, ops, null, opts)
  if (!cb) cb = noop

  ops = xtend(ops, opts) // clone
  if (/\*|(^,)fuse-bindings(,$)/.test(process.env.DEBUG)) ops.options = ['debug'].concat(ops.options || [])
  mnt = path.resolve(mnt)

  if (ops.displayFolder && IS_OSX) { // only works on osx
    if (!ops.options) ops.options = []
    ops.options.push('volname=' + path.basename(mnt))
    if (HAS_FOLDER_ICON) ops.options.push('volicon=' + OSX_FOLDER_ICON)
  }

  var callback = function (err) {
    callback = noop
    setImmediate(cb.bind(null, err))
  }

  var init = ops.init || call
  ops.init = function (next) {
    callback()
    if (init.length > 1) init(mnt, next) // backwards compat for now
    else init(next)
  }

  var error = ops.error || call
  ops.error = function (next) {
    callback(new Error('Mount failed'))
    error(next)
  }

  if (!ops.getattr) { // we need this for unmount to work on osx
    ops.getattr = function (path, cb) {
      if (path !== '/') return cb(fuse.EPERM)
      cb(null, {mtime: new Date(0), atime: new Date(0), ctime: new Date(0), mode: 16877, size: 4096})
    }
  }

  var mount = function () {
    // TODO: I got a feeling this can be done better
    if (os.platform() !== 'win32') {
      fs.stat(mnt, function (err, stat) {
        if (err) return cb(new Error('Mountpoint does not exist'))
        if (!stat.isDirectory()) return cb(new Error('Mountpoint is not a directory'))
        fs.stat(path.join(mnt, '..'), function (_, parent) {
          if (parent && parent.dev !== stat.dev) return cb(new Error('Mountpoint in use'))
          fuse.mount(mnt, ops)
        })
      })
    } else {
      fuse.mount(mnt, ops)
    }
  }

  if (!ops.force) return mount()
  exports.unmount(mnt, mount)
}

exports.unmount = function (mnt, cb) {
  fuse.unmount(path.resolve(mnt), cb)
}

exports.errno = function (code) {
  return (code && exports[code.toUpperCase()]) || -1
}

exports.EPERM = -1
exports.ENOENT = -2
exports.ESRCH = -3
exports.EINTR = -4
exports.EIO = -5
exports.ENXIO = -6
exports.E2BIG = -7
exports.ENOEXEC = -8
exports.EBADF = -9
exports.ECHILD = -10
exports.EAGAIN = -11
exports.ENOMEM = -12
exports.EACCES = -13
exports.EFAULT = -14
exports.ENOTBLK = -15
exports.EBUSY = -16
exports.EEXIST = -17
exports.EXDEV = -18
exports.ENODEV = -19
exports.ENOTDIR = -20
exports.EISDIR = -21
exports.EINVAL = -22
exports.ENFILE = -23
exports.EMFILE = -24
exports.ENOTTY = -25
exports.ETXTBSY = -26
exports.EFBIG = -27
exports.ENOSPC = -28
exports.ESPIPE = -29
exports.EROFS = -30
exports.EMLINK = -31
exports.EPIPE = -32
exports.EDOM = -33
exports.ERANGE = -34
exports.EDEADLK = -35
exports.ENAMETOOLONG = -36
exports.ENOLCK = -37
exports.ENOSYS = -38
exports.ENOTEMPTY = -39
exports.ELOOP = -40
exports.EWOULDBLOCK = -11
exports.ENOMSG = -42
exports.EIDRM = -43
exports.ECHRNG = -44
exports.EL2NSYNC = -45
exports.EL3HLT = -46
exports.EL3RST = -47
exports.ELNRNG = -48
exports.EUNATCH = -49
exports.ENOCSI = -50
exports.EL2HLT = -51
exports.EBADE = -52
exports.EBADR = -53
exports.EXFULL = -54
exports.ENOANO = -55
exports.EBADRQC = -56
exports.EBADSLT = -57
exports.EDEADLOCK = -35
exports.EBFONT = -59
exports.ENOSTR = -60
exports.ENODATA = -61
exports.ETIME = -62
exports.ENOSR = -63
exports.ENONET = -64
exports.ENOPKG = -65
exports.EREMOTE = -66
exports.ENOLINK = -67
exports.EADV = -68
exports.ESRMNT = -69
exports.ECOMM = -70
exports.EPROTO = -71
exports.EMULTIHOP = -72
exports.EDOTDOT = -73
exports.EBADMSG = -74
exports.EOVERFLOW = -75
exports.ENOTUNIQ = -76
exports.EBADFD = -77
exports.EREMCHG = -78
exports.ELIBACC = -79
exports.ELIBBAD = -80
exports.ELIBSCN = -81
exports.ELIBMAX = -82
exports.ELIBEXEC = -83
exports.EILSEQ = -84
exports.ERESTART = -85
exports.ESTRPIPE = -86
exports.EUSERS = -87
exports.ENOTSOCK = -88
exports.EDESTADDRREQ = -89
exports.EMSGSIZE = -90
exports.EPROTOTYPE = -91
exports.ENOPROTOOPT = -92
exports.EPROTONOSUPPORT = -93
exports.ESOCKTNOSUPPORT = -94
exports.EOPNOTSUPP = -95
exports.EPFNOSUPPORT = -96
exports.EAFNOSUPPORT = -97
exports.EADDRINUSE = -98
exports.EADDRNOTAVAIL = -99
exports.ENETDOWN = -100
exports.ENETUNREACH = -101
exports.ENETRESET = -102
exports.ECONNABORTED = -103
exports.ECONNRESET = -104
exports.ENOBUFS = -105
exports.EISCONN = -106
exports.ENOTCONN = -107
exports.ESHUTDOWN = -108
exports.ETOOMANYREFS = -109
exports.ETIMEDOUT = -110
exports.ECONNREFUSED = -111
exports.EHOSTDOWN = -112
exports.EHOSTUNREACH = -113
exports.EALREADY = -114
exports.EINPROGRESS = -115
exports.ESTALE = -116
exports.EUCLEAN = -117
exports.ENOTNAM = -118
exports.ENAVAIL = -119
exports.EISNAM = -120
exports.EREMOTEIO = -121
exports.EDQUOT = -122
exports.ENOMEDIUM = -123
exports.EMEDIUMTYPE = -124
