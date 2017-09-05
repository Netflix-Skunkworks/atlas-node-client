#!/usr/bin/env node

'use strict';

var fs = require('fs');
var path = require('path');

var COVERAGE_LINE_ID = '[manual coverage]';
var START_CHAR = '/coverage-';
var END_CHAR = ')]()';

var report = require('./../coverage/coverage-summary.json');
var pct = Math.round(report.total.lines.pct);
var color;

if (pct > 80) {
  color = 'green';
} else if (pct > 60) {
  color = 'yellow';
} else {
  color = 'red';
}

// read in the README
var README_PATH = path.join(__dirname, '../README.md');
var originalReadmeStr = fs.readFileSync(README_PATH).toString();
// process it
var out = processLines(originalReadmeStr);
// now write it back out
fs.writeFileSync(README_PATH, out);


function processLines(readmeStr) {

  var lines = readmeStr.toString().split('\n');
  var outLines = '';

  lines.forEach(function(line) {
    if (line.indexOf(COVERAGE_LINE_ID) > -1) {
      var startIdx = line.indexOf(START_CHAR);
      var endIdx = line.indexOf(END_CHAR);

      var newLine = [
          line.slice(0, startIdx + 1),
          'coverage-' + pct + '%25-' + color + '.svg',
          line.slice(endIdx),
          '\n'
      ].join('');

      outLines += newLine;
    } else {
      outLines += line + '\n';
    }
  });

  // chop off last newline
  return outLines.slice(0, -1);
}
