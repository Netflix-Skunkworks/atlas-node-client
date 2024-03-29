'use strict';

const metrics = require('../node-metrics.js');
const chai = require('chai');
const assert = chai.assert;

describe('metrics', () => {
  it('should measure event loop tick', (done) => {
    // on github actions test runners, this can take longer than 50ms
    var ms = 75;
    var threshold = 10e6; // value is within 10ms
    var fakeSelf = {
      eventLoopTime: {
        record: (time) => {
          assert(time[0] === 0, 'should be less than a second');
          assert(
            time[1] + threshold > ms * 1e6 &&
            time[1] - threshold < ms * 1e6,
            'nanosecond should be within tolerable threshold (time=' +
            time[1] + ')');
          done();
        }
      }
    };
    metrics._measureEventLoopTime(fakeSelf);
    // Introduce ms worth of event loop work between invocations of setImmediate
    setImmediate(() => {
      var start = Date.now();

      while (Date.now() - start < ms) {} // eslint-disable-line no-empty
    });
  });
});
