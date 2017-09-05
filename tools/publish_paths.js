'use strict';

/* eslint-disable no-console, no-process-exit */

const fs = require('fs');
const path = require('path');
const pkgjsonPath = path.join(__dirname, '../package.json');
const action = process.argv.length === 3 && process.argv[2];
const pkgjson = JSON.parse(fs.readFileSync(pkgjsonPath).toString());
const nodeABI = process.versions.modules;
const commonPath = 'atlas-native-client/v' + pkgjson.version +
  '/Release/atlas-v' + pkgjson.version +
  '-node-v' + nodeABI + '-' +
  process.platform + '-x64.tar.gz';

if (action === 'stage') {
  console.log(path.join('build/stage', commonPath));
} else if (action === 'url') {
  console.log('http://artifacts.netflix.com/' + commonPath);
} else {
  console.log('Unknown action', action);
  process.exit(1);
}
