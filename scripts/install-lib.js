'use strict';

const exec = require('child_process').exec;
const read = require('fs').readFileSync;
const pkgJson = JSON.parse(read('package.json'));
const cmd = 'sh ./scripts/install-lib-from-gh.sh ' + pkgJson.native_version;

const child = exec(cmd);
child.stdout.on('data', function(data) {
  process.stdout.write(data);
});

child.stderr.on('data', function(data) {
  process.stderr.write('\x1b[31m' + data + '\x1b[0m');
});



