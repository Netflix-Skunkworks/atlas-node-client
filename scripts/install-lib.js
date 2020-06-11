'use strict';

const exec = require('child_process').exec;
const read = require('fs').readFileSync;
const pkgJson = JSON.parse(read('package.json'));
const cmd = 'sh ./scripts/install-lib-from-gh.sh ' + pkgJson.native_version;

exec(cmd, (error, stdout, stderr) => {
  if (error) {
    throw error;
  }

  if (stderr) {
    process.stderr.write('\x1b[31m' + stderr + '\x1b[0m');
  }

  if (stdout) {
    process.stdout.write(stdout);
  }
});

