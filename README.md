# fuse-bindings

Fully maintained fuse bindings for Node that aims to cover the entire FUSE api

```
npm install fuse-bindings
```

Compared to [fuse4js](https://github.com/bcle/fuse4js) these bindings cover almost the entire FUSE api (except for locking) and doesn't do
any buffer copys in read/write. It also supports unmount and mouting of multiple fuse drives.

## Requirements

You need to have FUSE installed (or Dokany on Windows)

* On Linux/Ubuntu `sudo apt-get install libfuse-dev`
* On OSX
  * if you use Brew, install [OSXFuse](http://osxfuse.github.com/) and `brew install pkg-config`
  * if you use MacPorts, `sudo port install osxfuse +devel`
* On Windows see `Windows` down below...

### Windows
**WARNING**: Dokany is still not quite stable. It can cause BSODs. Be careful.

Using this on Windows is slightly more complicated. You need to install [Dokany](https://github.com/dokan-dev/dokany) (for `dokanfuse.lib`, `dokanctl.exe`, driver and service) **and** clone its repo (for the headers).

Once the Dokany repo is cloned, you also need to set environment variable `DOKAN_INSTALL_DIR` to the path to `DokenLibrary` of your Dokany installaton, and `DOKAN_FUSE_INCLUDE` to the path to `*dokany repo*\dokan_fuse\include`.

## Usage

Try creating an empty folder called `mnt` and run the below example

``` js
var fuse = require('fuse-bindings')

fuse.mount('./mnt', {
  readdir: function (path, cb) {
    console.log('readdir(%s)', path)
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

#### `fuse.mount(mnt, ops, [cb])`

Mount a new filesystem on `mnt`.
Pass the FUSE operations you want to support as the `ops` argument.

#### `fuse.unmount(mnt, [cb])`

Unmount a filesystem

#### `fuse.context()`

Returns the current fuse context (pid, uid, gid).
Must be called inside a fuse callback.

## Mount options

#### `ops.options`

Set [mount options](http://blog.woralelandia.com/2012/07/16/fuse-mount-options/)

``` js
ops.options = ['direct_io'] // set the direct_io option
```

#### `ops.displayFolder`

Set to `true` to make OSX display a folder icon and the folder name as the mount point in finder

#### `ops.force`

Set to `true` to force mount the filesystem (will do an unmount first)

## FUSE operations

Most of the [FUSE api](http://fuse.sourceforge.net/doxygen/structfuse__operations.html) is supported. In general the callback for each op should be called with `cb(returnCode, [value])` where the return code is a number (`0` for OK and `< 0` for errors). See below for a list of POSIX error codes.

#### `ops.init(cb)`

Called on filesystem init.

#### `ops.access(path, mode, cb)`

Called before the filesystem accessed a file

#### `ops.statfs(path, cb)`

Called when the filesystem is being stat'ed. Accepts a fs stat object after the return code in the callback.

``` js
ops.statfs = function (path, cb) {
  cb(0, {
    bsize: 1000000,
    frsize: 1000000,
    blocks: 1000000,
    bfree: 1000000,
    bavail: 1000000,
    files: 1000000,
    ffree: 1000000,
    favail: 1000000,
    fsid: 1000000,
    flag: 1000000,
    namemax: 1000000
  })
}
```

#### `ops.getattr(path, cb)`

Called when a path is being stat'ed. Accepts a stat object (similar to the one returned in `fs.stat(path, cb)`) after the return code in the callback.

``` js
ops.getattr = function (path, cb) {
  cb(0, {
    mtime: new Date(),
    atime: new Date(),
    ctime: new Date(),
    size: 100,
    mode: 16877,
    uid: process.getuid(),
    gid: process.getgid()
  })
}
```

#### `ops.fgetattr(path, fd, cb)`

Same as above but is called when someone stats a file descriptor

#### `ops.flush(path, fd, cb)`

Called when a file descriptor is being flushed

#### `ops.fsync(path, fd, datasync, cb)`

Called when a file descriptor is being fsync'ed.

#### `ops.fsyncdir(path, fd, datasync, cb)`

Same as above but on a directory

#### `ops.readdir(path, cb)`

Called when a directory is being listed. Accepts an array of file/directory names after the return code in the callback

``` js
ops.readdir = function (path, cb) {
  cb(0, ['file-1.txt', 'dir'])
}
```

#### `ops.truncate(path, size, cb)`

Called when a path is being truncated to a specific size

#### `ops.ftruncate(path, fd, size, cb)`

Same as above but on a file descriptor

#### `ops.readlink(path, cb)`

Called when a symlink is being resolved. Accepts a pathname (that the link should resolve to) after the return code in the callback

``` js
ops.readlink = function (path, cb) {
  cb(null, 'file.txt') // make link point to file.txt
}
```

#### `ops.chown(path, uid, gid, cb)`

Called when ownership of a path is being changed

#### `ops.chmod(path, mode, cb)`

Called when the mode of a path is being changed

#### `ops.mknod(path, mode, dev, cb)`

Called when the a new device file is being made.

#### `ops.setxattr(path, name, buffer, length, offset, flags, cb)`

Called when extended attributes is being set (see the extended docs for your platform).
Currently you can read the attribute value being set in `buffer` at `offset`.

#### `ops.getxattr(path, name, buffer, length, offset, cb)`

Called when extended attributes is being read.
Currently you have to write the result to the provided `buffer` at `offset`.

#### `ops.open(path, flags, cb)`

Called when a path is being opened. `flags` in a number containing the permissions being requested. Accepts a file descriptor after the return code in the callback.

``` js
var toFlag = function(flags) {
  flags = flags & 3
  if (flags === 0) return 'r'
  if (flags === 1) return 'w'
  return 'r+'
}

ops.open = function (path, flags, cb) {
  var flag = toFlag(flags) // convert flags to a node style string
  ...
  cb(0, 42) // 42 is a file descriptor
}
```

#### `ops.opendir(path, flags, cb)`

Same as above but for directories

#### `ops.read(path, fd, buffer, length, position, cb)`

Called when contents of a file is being read. You should write the result of the read to the `buffer` and return the number of bytes written as the first argument in the callback.
If no bytes were written (read is complete) return 0 in the callback.

``` js
var data = new Buffer('hello world')

ops.read = function (path, fd, buffer, length, position, cb) {
  if (position >= data.length) return cb(0) // done
  var part = data.slice(position, position + length)
  part.copy(buffer) // write the result of the read to the result buffer
  cb(part.length) // return the number of bytes read
}
```

#### `ops.write(path, fd, buffer, length, position, cb)`

Called when a file is being written to. You can get the data being written in `buffer` and you should return the number of bytes written in the callback as the first argument.

``` js
ops.write = function (path, fd, buffer, length, position, cb) {
  console.log('writing', buffer.slice(0, length))
  cb(length) // we handled all the data
}
```

#### `ops.release(path, fd, cb)`

Called when a file descriptor is being released. Happens when a read/write is done etc.

#### `ops.releasedir(path, fd, cb)`

Same as above but for directories

#### `ops.create(path, mode, cb)`

Called when a new file is being opened.

#### `ops.utimens(path, atime, mtime, cb)`

Called when the atime/mtime of a file is being changed.

#### `ops.unlink(path, cb)`

Called when a file is being unlinked.

#### `ops.rename(src, dest, cb)`

Called when a file is being renamed.

#### `ops.link(src, dest, cb)`

Called when a new link is created.

#### `ops.symlink(src, dest, cb)`

Called when a new symlink is created

#### `ops.mkdir(path, mode, cb)`

Called when a new directory is being created

#### `ops.rmdir(path, cb)`

Called when a directory is being removed

#### `ops.destroy(cb)`

Both `read` and `write` passes the underlying fuse buffer without copying them to be as fast as possible.

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
