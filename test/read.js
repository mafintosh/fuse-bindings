var mnt = require('./fixtures/mnt')
var stat = require('./fixtures/stat')
var fuse = require('../')
var tape = require('tape')
var fs = require('fs')
var path = require('path')
var concat = require('concat-stream')

tape('read', function (t) {
  var ops = {
    force: true,
    readdir: function (path, cb) {
      if (path === '/') return cb(null, ['test'])
      return cb(fuse.ENOENT)
    },
    getattr: function (path, cb) {
      if (path === '/') return cb(null, stat({mode: 'dir', size: 4096}))
      if (path === '/test') return cb(null, stat({mode: 'file', size: 11}))
      return cb(fuse.ENOENT)
    },
    open: function (path, flags, cb) {
      cb(0, 42)
    },
    release: function (path, fd, cb) {
      t.same(fd, 42, 'fd was passed to release')
      cb(0)
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

    fs.readFile(path.join(mnt, 'test'), function (err, buf) {
      t.error(err, 'no error')
      t.same(buf, new Buffer('hello world'), 'read file')

      fs.readFile(path.join(mnt, 'test'), function (err, buf) {
        t.error(err, 'no error')
        t.same(buf, new Buffer('hello world'), 'read file again')

        fs.createReadStream(path.join(mnt, 'test'), {start: 0, end: 4}).pipe(concat(function (buf) {
          t.same(buf, new Buffer('hello'), 'partial read file')

          fs.createReadStream(path.join(mnt, 'test'), {start: 6, end: 10}).pipe(concat(function (buf) {
            t.same(buf, new Buffer('world'), 'partial read file + start offset')

            fuse.unmount(mnt, function () {
              t.end()
            })
          }))
        }))
      })
    })
  })
})
