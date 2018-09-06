// this tests whether a problematic set of metrics causes a crash
// the metrics-test.txt file includes counters and timers with the same
// name
'use strict';


function processMetric(atlas, type, name, tags, value) {
  switch (type) {
    case 'TIMER':
      const seconds = 0.0;
      const nanos = value * 1e6;
      atlas.timer(name, tags).record(seconds, nanos);
      break;
    case 'GAUGE':
      atlas.gauge(name, tags).update(value);
      break;
    case 'COUNTER':
      atlas.dcounter(name, tags).increment(value);
      break;
    default:
      console.log('Unknown type: ' + type);
      const payloadErrors = atlas.counter('atlas.agent.errors', {
        err: 'payload'
      });

      payloadErrors.increment();
      return false;
  }
  atlas.counter('atlas.agent.processed').increment();
  return true;
}

describe('atlas timers/counters', () => {
  it('should not crash', (done) => {
    const lineReader = require('readline').createInterface({
      input: require('fs').createReadStream('test/metrics-test.txt')
    });

    const atlas = require('../');
    lineReader.on('line', function (line) {
      const entry = JSON.parse(line);
      console.log('Handling ', entry);
      processMetric(atlas, entry.type, entry.name, entry.tags || {}, entry.value);
    });

    lineReader.on('close', function () {
      console.log("Done!");
      done();
    });
  });
});
