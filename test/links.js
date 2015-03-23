var mnt = require('./fixtures/mnt')
var stat = require('./fixtures/stat')
var fuse = require('../')
var tape = require('tape')
var fs = require('fs')
var path = require('path')

tape('readlink', function (t) {
  var ops = {
    force: true,
    readdir: function (path, cb) {
      if (path === '/') return cb(null, ['hello', 'link'])
      return cb(fuse.ENOENT)
    },
    readlink: function (path, cb) {
      cb(0, 'hello')
    },
    getattr: function (path, cb) {
      if (path === '/') return cb(null, stat({mode: 'dir', size: 4096}))
      if (path === '/hello') return cb(null, stat({mode: 'file', size: 11}))
      if (path === '/link') return cb(null, stat({mode: 'link', size: 5}))
      return cb(fuse.ENOENT)
    },
    open: function (path, flags, cb) {
      cb(0, 42)
    },
    read: function (path, fd, buf, len, pos, cb) {
      var str = 'hello world'.slice(pos, pos + len)
      if (!str) return cb(0)
      buf.write(str)
      return cb(str.length)
    }
  }

  fuse.mount(mnt, ops, function (err) {
    t.error(err, 'no error')

    fs.lstat(path.join(mnt, 'link'), function (err, stat) {
      t.error(err, 'no error')
      t.same(stat.size, 5, 'correct size')

      fs.stat(path.join(mnt, 'hello'), function (err, stat) {
        t.error(err, 'no error')
        t.same(stat.size, 11, 'correct size')

        fs.readlink(path.join(mnt, 'link'), function (err, dest) {
          t.error(err, 'no error')
          t.same(dest, 'hello', 'link resolves')

          fs.readFile(path.join(mnt, 'link'), function (err, buf) {
            t.error(err, 'no error')
            t.same(buf, new Buffer('hello world'), 'can read link content')

            fuse.unmount(mnt, function () {
              t.end()
            })
          })
        })
      })
    })
  })
})
