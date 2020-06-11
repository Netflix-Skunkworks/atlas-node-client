'use strict';

const atlas = require('../');
const chai = require('chai');
const assert = chai.assert;
const expect = chai.expect;

describe('atlas extension', () => {
  it('should provide counters', () => {
    let counter = atlas.counter('foo');
    assert.equal(counter.increment(), undefined);
    assert.equal(counter.count(), 1);
    assert.equal(counter.add(3), undefined);
    assert.equal(counter.count(), 4);

    let counter3 = atlas.counter('foo', {
      k: 'v2'
    });
    assert.equal(counter3.increment(), undefined);
    assert.equal(counter3.count(), 1);
  });

  it('should handle counter.increment(number)', () => {
    let counter = atlas.counter('incr_num');
    counter.increment(2);
    assert.equal(counter.count(), 2);
  });

  it('should provide double counters', () => {
    let counter = atlas.dcounter('fooDouble');
    assert.equal(counter.increment(), undefined);
    assert.equal(counter.count(), 1);
    assert.equal(counter.add(0.42), undefined);
    assert.equal(counter.count(), 1.42);
  });

  it('should handle fractional increments', () => {
    let ic = atlas.counter('int_counter');
    let dc = atlas.dcounter('double_counter');
    let counters = [ ic, dc ];

    for (let counter of counters) {
      counter.increment(0.3);
      counter.increment(0.3);
      counter.increment(0.3);
      counter.increment(0.3);
    }
    assert.equal(ic.count(), 0);
    assert.equal(dc.count(), 1.2);
  });

  const NANOS = 1000 * 1000 * 1000;
  it('should provide timers', () => {
    let timer = atlas.timer('t');
    // 50 microseconds
    assert.equal(timer.record(1, 50 * 1000), undefined);
    assert.equal(timer.count(), 1);
    assert.equal(timer.totalTime(), 1 + 50000 / NANOS);
    assert.equal(timer.record(0, 3 * 1000), undefined);
    assert.equal(timer.count(), 2);
    assert.equal(timer.totalTime(), 1 + 53000 / NANOS);
  });

  it('should provide a nice timeThis wrapper', () => {
    let value = atlas.timer('time.this.example').timeThis(() => {
      let j = 0;

      for (let i = 0; i < 1000000; ++i) {
        j++;
      }
      return j;
    });

    assert.equal(value, 1000000);
    let timer = atlas.timer('time.this.example');
    assert.equal(timer.count(), 1);
    assert(timer.totalTime() > 0, 'Some time was recorded');
  });

  it('should provide gauges that use the last value set', () => {
    let g = atlas.gauge('gauge.example');
    g.update(42);
    assert.equal(g.value(), 42);
  });

  it('should maybe have a max gauge (integer)', () => {
    let mg = atlas.maxGauge('gauge.max', {
      k: 'v'
    });
    assert.equal(mg.value(), 0);

    mg.update(2);
    assert.equal(mg.value(), 2);

    mg.update(3);
    assert.equal(mg.value(), 3);

    mg.update(1);
    assert.equal(mg.value(), 3);
  });

  it('should have distribution summaries', () => {
    let ds = atlas.distSummary('dist.summary');

    assert.equal(ds.count(), 0);
    assert.equal(ds.totalAmount(), 0);

    ds.record(1000);

    assert.equal(ds.count(), 1);
    assert.equal(ds.totalAmount(), 1000);

    ds.record(1001);
    assert.equal(ds.count(), 2);
    assert.equal(ds.totalAmount(), 2001);
  });

  it('should provide a scope decorator that automatically adds tags', () => {
    let s = atlas.scope({
      foo: 'bar'
    });
    let ctr = s.counter('scoped.name1', {
      key: 'v1'
    });
    ctr.increment();

    assert.equal(atlas.counter(
      'scoped.name1', {
        key: 'v1',
        foo: 'bar'
      }).count(), 1);
  });

  it('should help time async operations', (mochaDone) => {
    atlas.timer('timer.async').timeAsync((done) => {
      setTimeout(() => {
        done();
        let t = atlas.timer('timer.async');
        assert.equal(t.count(), 1);
        assert(t.totalTime() > 0);
        mochaDone();
      }, 0);
    });
  });

  it('should have long task timers', () => {
    let t = atlas.longTaskTimer('long.task.timer');
    let id = t.start();
    assert(id >= 0);
    let j = 0;

    for (let i = 0; i < 1000000; ++i) {
      j++;
    }
    assert(j > 0);
    assert.equal(t.activeTasks(), 1);
    let duration = t.stop(id);
    assert(duration > 0);
    assert.equal(t.activeTasks(), 0);
  });

  it('should provide an age gauge', (done) => {
    atlas.setUpdateInterval(1);
    let a = atlas.age('age.gauge');
    setTimeout(() => {
      assert(a.value() >= 0.01); // in seconds
      done();
    }, 20);
  });

  it('should provide bucket counters', () => {
    let bc = atlas.bucketCounter(
      'example.bucket.c', {
        function: 'bytes',
        value: 1024
      });
    bc.record(1000);
    let c = atlas.counter('example.bucket.c', {
      bucket: '1024_B'
    });
    assert.equal(c.count(), 1);
    bc.record(1000);
    assert.equal(c.count(), 2);
    bc.record(212);
    let c2 = atlas.counter('example.bucket.c', {
      bucket: '0256_B'
    });
    assert.equal(c2.count(), 1);
  });

  it('should provide bucket timers', () => {
    let bt = atlas.bucketTimer(
      'example.bucket.t', {
        function: 'latency',
        value: 3,
        unit: 's'
      });
    bt.record(1, 0);
    let t = atlas.timer('example.bucket.t', {
      bucket: '1500ms'
    });
    assert.equal(t.count(), 1);
    bt.record(1, 200000);
    assert.equal(t.count(), 2);
    bt.record(0, 212 * 1000);
    let t2 = atlas.timer('example.bucket.t', {
      bucket: '0375ms'
    });
    assert.equal(t2.count(), 1);
  });

  it('should provide bucket distribution summaries', () => {
    let bc = atlas.bucketDistSummary(
      'example.bucket.d', {
        function: 'decimal',
        value: 20000
      });
    bc.record(15761);
    let c = atlas.distSummary('example.bucket.d', {
      bucket: '20_k'
    });
    assert.equal(c.count(), 1);
    bc.record(12030, 0);
    assert.equal(c.count(), 2);
    bc.record(20001, 0);
    let c2 = atlas.distSummary('example.bucket.d', {
      bucket: 'large'
    });
    assert.equal(c2.count(), 1);
  });

  it('should provide percentile timers', () => {
    let samples = 20000;
    let d = atlas.percentileTimer('example.perc.t');

    for (let i = 0; i < samples; ++i) {
      d.record(0, i * 1000 * 1000.0); // record in milliseconds
    }
    assert.equal(d.count(), samples);

    // millis to seconds
    assert.equal(d.totalTime(), (samples - 1) * samples / 2 / 1000);

    for (let i = 0; i <= 100; ++i) {
      let expected = (samples / 100) * i / 1000.0;
      let threshold = 0.15 * expected;
      assert.approximately(d.percentile(i), expected, threshold);
    }
  });

  it('should provide percentile distribution summaries', () => {
    let samples = 20000;
    let d = atlas.percentileDistSummary('example.perc.d');

    for (let i = 0; i < samples; ++i) {
      d.record(i);
    }
    assert.equal(d.count(), samples);
    assert.equal(d.totalAmount(), (samples - 1) * samples / 2);

    for (let i = 0; i <= 100; ++i) {
      let expected = (samples / 100) * i;
      let threshold = 0.15 * expected;
      assert.approximately(d.percentile(i), expected, threshold);
    }
  });

  it('should provide interval counters', () => {
    let d = atlas.intervalCounter('example.counter.d');

    d.increment();
    d.increment(10);

    assert.equal(d.count(), 11);
  });

  it('should give some debugging info', () => {
    let d = atlas.getDebugInfo();
    expect(d).to.have.all.keys('config', 'measurements');
    assert.isObject(d.config);
    assert.isArray(d.measurements);
    assert.isAtLeast(d.measurements.length, 2);
  });

  it('should validate metrics when in dev mode', () => {
    atlas.setDevMode(true);
    const noName = () => {
      atlas.counter('');
    };
    assert.throw(noName, /empty/);

    const invalidTagKey = () => {
      const tags = {};
      tags['a'.repeat(70)] = 'v';
      atlas.timer('foo', tags);
    };
    assert.throw(invalidTagKey, /exceeds length/);

    const invalidTagVal = () => {
      atlas.timer('foo', {k: 'a'.repeat(160)});
    };
    assert.throw(invalidTagVal, /exceeds length/);

    const tooManyTags = () => {
      const tags = {};

      for (let i = 0; i < 22; ++i) {
        tags[i] = 'v';
      }
      atlas.counter('f', tags);
    };
    assert.throw(tooManyTags, /user tags/);

    const reserved = () => {
      atlas.counter('foo', {'atlas.test': 'foo'});
    };
    assert.throw(reserved, /reserved namespace/);

  });

  it('should not throw when in prod', () => {
    atlas.setDevMode(false);

    // empty name
    atlas.counter('');

    // empty key
    atlas.counter('foo', { '': undefined });

    // empty val
    atlas.counter('foo', { k: '' });

    const invalidTagKey = () => {
      const tags = {};
      tags['a'.repeat(70)] = 'v';
      atlas.timer('foo', tags);
    };
    invalidTagKey();

    const invalidTagVal = () => {
      atlas.timer('foo', {k: 'a'.repeat(160)});
    };
    invalidTagVal();

    const tooManyTags = () => {
      const tags = {};

      for (let i = 0; i < 22; ++i) {
        tags[i] = 'v';
      }
      atlas.counter('f', tags);
    };
    tooManyTags();

    const reserved = () => {
      atlas.counter('foo', {'atlas.test': 'foo'});
    };
    reserved();

  });

  it('should validate metric IDs', () => {
    const noName = atlas.validateNameAndTags('');
    assert.equal(noName.length, 1);
    assert.match(noName[0].ERROR, /'name'/); // complains about 'name'

    // empty key
    const emptyKey = atlas.validateNameAndTags('foo', { '': undefined });
    assert.equal(emptyKey.length, 1);
    assert.match(emptyKey[0].ERROR, /empty/i);

    // empty val
    const emptyVal = atlas.validateNameAndTags('foo', { k: '' });
    assert.equal(emptyVal.length, 1);
    assert.match(emptyVal[0].ERROR, /key 'k' cannot be empty/i);

    {
      const key = 'a'.repeat(70);
      const tags = {};
      tags[key] = 'v';
      const keyTooLong = atlas.validateNameAndTags('foo', tags);
      assert.equal(keyTooLong.length, 1);
      assert.match(keyTooLong[0].ERROR, /aaaa' exceeds/i);
    }

    const valTooLong = atlas.validateNameAndTags('foo', {k: 'a'.repeat(160)});
    assert.equal(valTooLong.length, 1);
    assert.match(valTooLong[0].ERROR, /aaa' for key 'k' exceeds/i);

    {
      const tags = {};

      for (let i = 0; i < 22; ++i) {
        tags[i] = 'v';
      }
      const tooManyTags = atlas.validateNameAndTags('f', tags);
      assert.equal(tooManyTags.length, 1);
      // 22 tags + name > 20
      assert.match(tooManyTags[0].ERROR, /too many user tags.*23/i);
    }

    const reserved = atlas.validateNameAndTags('foo', {'atlas.test': 'foo'});
    assert.equal(reserved.length, 1);
    assert.match(reserved[0].ERROR, /reserved namespace/i);

    // warning only
    const warnings = atlas.validateNameAndTags('foo ', {'bar#1': 'val%2'});
    assert.equal(warnings.length, 3);

    for (const issue of warnings) {
      assert.match(issue.WARN, /encoded as/);
    }
  });
});
