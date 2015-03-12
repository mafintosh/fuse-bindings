# fuse-bindings

Fully maintained fuse bindings for Node that aims to cover the entire FUSE api

```
npm install fuse-bindings
```

Created because [fuse4js](https://github.com/bcle/fuse4js) isn't maintained anymore and doesn't compile on iojs

## Usage

Try creating an empty folder called `mnt` and run the below example

``` js
var fuse = require('fuse-bindings')

fuse.mount('./mnt', {
  readdir: function (path, cb) {
    if (path === '/') return cb(0, ['test'])
    cb(0)
  },
  getattr: function (path, cb) {
    console.log('getattr(%s)', path)
    if (path === '/') {
      cb(0, {
        mtime: new Date(),
        atime: new Date(),
        ctime: new Date(),
        size: 100,
        mode: 16877,
        uid: process.getuid(),
        gid: process.getgid()
      })
      return
    }

    if (path === '/test') {
      cb(0, {
        mtime: new Date(),
        atime: new Date(),
        ctime: new Date(),
        size: 12,
        mode: 33188,
        uid: process.getuid(),
        gid: process.getgid()
      })
      return
    }

    cb(fuse.ENOENT)
  },
  open: function (path, flags, cb) {
    console.log('open(%s, %d)', path, flags)
    cb(0, 42) // 42 is an fd
  },
  read: function (path, fd, buf, len, pos, cb) {
    console.log('read(%s, %d, %d, %d)', path, fd, len, pos)
    var str = 'hello world\n'.slice(pos)
    if (!str) return cb(0)
    buf.write(str)
    return cb(str.length)
  }
})

process.on('SIGINT', function () {
  fuse.unmount('./mnt', function () {
    process.exit()
  })
})
```

## API

#### `fuse.mount(mnt, ops)`

Mount a new filesystem on `mnt`.
Pass the FUSE operations you want to support as the `ops` argument.

#### `fuse.unmount(mnt, [cb])`

Unmount a filesystem

#### `fuse.unmountSync(mnt)`

Unmount a filesystem synchroniously

## FUSE operations

Most of the [FUSE api](http://fuse.sourceforge.net/doxygen/structfuse__operations.html) is supported.

* `ops.init(cb)`
* `ops.access(path, cb)`
* `ops.statfs(path, cb)`
* `ops.getattr(path, cb)`
* `ops.fgetattr(path, fd, cb)`
* `ops.flush(path, fd, cb)`
* `ops.fsync(path, fd, cb)`
* `ops.fsyncdir(path, fd, cb)`
* `ops.readdir(path, cb)`
* `ops.truncate(path, size, cb)`
* `ops.ftruncate(path, fd, size, cb)`
* `ops.readlink(path, result, cb)`
* `ops.chown(path, uid, gid, cb)`
* `ops.chmod(path, mode, cb)`
* `ops.setxattr(path, name, buf, length, offset, flags, cb)`
* `ops.getxattr(path, name, buf, length, offset, cb)`
* `ops.open(path, flags, cb)`
* `ops.opendir(path, flags, cb)`
* `ops.read(path, fd, buf, length, position, cb)`
* `ops.write(path, fd, buf, length, position, cb)`
* `ops.release(path, fd, cb)`
* `ops.releasedir(path, fd, cb)`
* `ops.create(path, mode, cb)`
* `ops.utimens(path, cb)`
* `ops.unlink(path, cb)`
* `ops.rename(src, dest, cb)`
* `ops.link(src, dest, cb)`
* `ops.symlink(src, dest, cb)`
* `ops.mkdir(path, cb)`
* `ops.rmdir(path, cb)`
* `ops.destroy(cb)`

The goal is to support 100% of the API.

When using `getattr` you should return a stat object after the status code,
`readdir` expects an array of directory names, `open` an optional fd and
`utimens` an object with `mtime` and `atime` dates.

Both `read` and `write` passes the underlying fuse buffer without copying them to be as fast as possible.

You can pass [mount options](http://blog.woralelandia.com/2012/07/16/fuse-mount-options/) (i.e. `direct_io` etc) as `ops.options = ['direct_io']`

## Error codes

The available error codes are exposes as well as properties. These include

* `fuse.EPERM === -1`
* `fuse.ENOENT === -2`
* `fuse.ESRCH === -3`
* `fuse.EINTR === -4`
* `fuse.EIO === -5`
* `fuse.ENXIO === -6`
* `fuse.E2BIG === -7`
* `fuse.ENOEXEC === -8`
* `fuse.EBADF === -9`
* `fuse.ECHILD === -10`
* `fuse.EAGAIN === -11`
* `fuse.ENOMEM === -12`
* `fuse.EACCES === -13`
* `fuse.EFAULT === -14`
* `fuse.ENOTBLK === -15`
* `fuse.EBUSY === -16`
* `fuse.EEXIST === -17`
* `fuse.EXDEV === -18`
* `fuse.ENODEV === -19`
* `fuse.ENOTDIR === -20`
* `fuse.EISDIR === -21`
* `fuse.EINVAL === -22`
* `fuse.ENFILE === -23`
* `fuse.EMFILE === -24`
* `fuse.ENOTTY === -25`
* `fuse.ETXTBSY === -26`
* `fuse.EFBIG === -27`
* `fuse.ENOSPC === -28`
* `fuse.ESPIPE === -29`
* `fuse.EROFS === -30`
* `fuse.EMLINK === -31`
* `fuse.EPIPE === -32`
* `fuse.EDOM === -33`
* `fuse.ERANGE === -34`
* `fuse.EDEADLK === -35`
* `fuse.ENAMETOOLONG === -36`
* `fuse.ENOLCK === -37`
* `fuse.ENOSYS === -38`
* `fuse.ENOTEMPTY === -39`
* `fuse.ELOOP === -40`
* `fuse.EWOULDBLOCK === -11`
* `fuse.ENOMSG === -42`
* `fuse.EIDRM === -43`
* `fuse.ECHRNG === -44`
* `fuse.EL2NSYNC === -45`
* `fuse.EL3HLT === -46`
* `fuse.EL3RST === -47`
* `fuse.ELNRNG === -48`
* `fuse.EUNATCH === -49`
* `fuse.ENOCSI === -50`
* `fuse.EL2HLT === -51`
* `fuse.EBADE === -52`
* `fuse.EBADR === -53`
* `fuse.EXFULL === -54`
* `fuse.ENOANO === -55`
* `fuse.EBADRQC === -56`
* `fuse.EBADSLT === -57`
* `fuse.EDEADLOCK === -35`
* `fuse.EBFONT === -59`
* `fuse.ENOSTR === -60`
* `fuse.ENODATA === -61`
* `fuse.ETIME === -62`
* `fuse.ENOSR === -63`
* `fuse.ENONET === -64`
* `fuse.ENOPKG === -65`
* `fuse.EREMOTE === -66`
* `fuse.ENOLINK === -67`
* `fuse.EADV === -68`
* `fuse.ESRMNT === -69`
* `fuse.ECOMM === -70`
* `fuse.EPROTO === -71`
* `fuse.EMULTIHOP === -72`
* `fuse.EDOTDOT === -73`
* `fuse.EBADMSG === -74`
* `fuse.EOVERFLOW === -75`
* `fuse.ENOTUNIQ === -76`
* `fuse.EBADFD === -77`
* `fuse.EREMCHG === -78`
* `fuse.ELIBACC === -79`
* `fuse.ELIBBAD === -80`
* `fuse.ELIBSCN === -81`
* `fuse.ELIBMAX === -82`
* `fuse.ELIBEXEC === -83`
* `fuse.EILSEQ === -84`
* `fuse.ERESTART === -85`
* `fuse.ESTRPIPE === -86`
* `fuse.EUSERS === -87`
* `fuse.ENOTSOCK === -88`
* `fuse.EDESTADDRREQ === -89`
* `fuse.EMSGSIZE === -90`
* `fuse.EPROTOTYPE === -91`
* `fuse.ENOPROTOOPT === -92`
* `fuse.EPROTONOSUPPORT === -93`
* `fuse.ESOCKTNOSUPPORT === -94`
* `fuse.EOPNOTSUPP === -95`
* `fuse.EPFNOSUPPORT === -96`
* `fuse.EAFNOSUPPORT === -97`
* `fuse.EADDRINUSE === -98`
* `fuse.EADDRNOTAVAIL === -99`
* `fuse.ENETDOWN === -100`
* `fuse.ENETUNREACH === -101`
* `fuse.ENETRESET === -102`
* `fuse.ECONNABORTED === -103`
* `fuse.ECONNRESET === -104`
* `fuse.ENOBUFS === -105`
* `fuse.EISCONN === -106`
* `fuse.ENOTCONN === -107`
* `fuse.ESHUTDOWN === -108`
* `fuse.ETOOMANYREFS === -109`
* `fuse.ETIMEDOUT === -110`
* `fuse.ECONNREFUSED === -111`
* `fuse.EHOSTDOWN === -112`
* `fuse.EHOSTUNREACH === -113`
* `fuse.EALREADY === -114`
* `fuse.EINPROGRESS === -115`
* `fuse.ESTALE === -116`
* `fuse.EUCLEAN === -117`
* `fuse.ENOTNAM === -118`
* `fuse.ENAVAIL === -119`
* `fuse.EISNAM === -120`
* `fuse.EREMOTEIO === -121`
* `fuse.EDQUOT === -122`
* `fuse.ENOMEDIUM === -123`
* `fuse.EMEDIUMTYPE === -124`

## License

MIT
