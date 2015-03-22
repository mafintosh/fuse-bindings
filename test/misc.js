var mnt = require('./fixtures/mnt')
var fuse = require('../')
var tape = require('tape')

tape('mount', function (t) {
  fuse.mount(mnt, {force: true}, function (err) {
    t.error(err, 'no error')
    t.ok(true, 'works')
    fuse.unmount(mnt, function () {
      t.end()
    })
  })
})

tape('mount + unmount + mount', function (t) {
  fuse.mount(mnt, {force: true}, function (err) {
    t.error(err, 'no error')
    t.ok(true, 'works')
    fuse.unmount(mnt, function () {
      fuse.mount(mnt, {force: true}, function (err) {
        t.error(err, 'no error')
        t.ok(true, 'works')
        fuse.unmount(mnt, function () {
          t.end()
        })
      })
    })
  })
})

tape('mnt point must exist', function (t) {
  fuse.mount(mnt + '.does-not-exist', {}, function (err) {
    t.ok(err, 'had error')
    t.end()
  })
})

tape('mnt point must be directory', function (t) {
  fuse.mount(__filename, {}, function (err) {
    t.ok(err, 'had error')
    t.end()
  })
})
