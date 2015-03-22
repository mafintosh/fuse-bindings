var os = require('os')
var path = require('path')
var fs = require('fs')

var mnt = path.join(os.tmpdir(), 'fuse-bindings-' + process.pid + '-' + Date.now())

try {
  fs.mkdirSync(mnt)
} catch (err) {
  // do nothing
}

module.exports = mnt
