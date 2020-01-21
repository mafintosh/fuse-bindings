var mnt = require('./fixtures/mnt')
var stat = require('./fixtures/stat')
var fuse = require('../')
var tape = require('tape')
var fs = require('fs')
var path = require('path')

tape('write', function (t) {
  var created = false
  var data = new Buffer(1024)
  var size = 0

  var ops = {
    force: true,
    readdir: function (path, cb) {
      if (path === '/') return cb(null, created ? ['hello'] : [])
      return cb(fuse.ENOENT)
    },
    truncate: function (path, size, cb) {
      cb(0)
    },
    getattr: function (path, cb) {
      if (path === '/') return cb(null, stat({mode: 'dir', size: 4096}))
      if (path === '/hello' && created) return cb(null, stat({mode: 'file', size: size}))
      return cb(fuse.ENOENT)
    },
    create: function (path, flags, cb) {
      t.ok(!created, 'file not created yet')
      created = true
      cb(0, 42)
    },
    release: function (path, fd, cb) {
      cb(0)
    },
    write: function (path, fd, buf, len, pos, cb) {
      buf.slice(0, len).copy(data, pos)
      size = Math.max(pos + len, size)
      cb(buf.length)
    }
  }

  fuse.mount(mnt, ops, function (err) {
    t.error(err, 'no error')

    fs.writeFile(path.join(mnt, 'hello'), 'hello world', function (err) {
      t.error(err, 'no error')
      t.same(data.slice(0, size), new Buffer('hello world'), 'data was written')

      fuse.unmount(mnt, function () {
        t.end()
      })
    })
  })
})
