'use strict';

const atlas = require('../mocks');
const assert = require('chai').assert;

describe('atlas mocks', () => {
  it('should mock counters', () => {
    let counter = atlas.counter('foo');
    counter.increment();
    assert(counter);
  });
  it('should mock interval counters', () => {
    let counter = atlas.intervalCounter('ic');
    counter.increment();
    assert(counter);
  });
  it('should mock timers', () => {
    let timer = atlas.timer('timer');
    timer.record(1, 100);
    assert(timer);
  });
  it('should mock gauges', () => {
    let g = atlas.gauge('gauge');
    g.update(42.0);
    assert(g);
  });
  it('should mock age gauges', () => {
    let g = atlas.age('age');
    g.update((new Date).getTime());
    assert(g);
  });
  it('should mock max gauges', () => {
    let g = atlas.maxGauge('mg', {
      k: 'v1'
    });
    g.update(1.0);
    g.update(0.0);
    g.update(2.0);
    assert(g);
  });
  it('should mock distribution summaries', () => {
    let ds = atlas.distSummary('ds');
    ds.record(42.0);
    assert(ds);
  });
  it('should mock long task timers', () => {
    let ltt = atlas.longTaskTimer('ltt');
    ltt.record(420.0);
    assert(ltt);
  });
  it('should mock percentile distribution summaries', () => {
    let pds = atlas.percentileDistSummary('pds');
    pds.record(1.0);
    assert(pds);
  });
  it('should mock percentile timers', () => {
    let pt = atlas.percentileTimer('pt');
    pt.record(420.0);
    assert(pt);
  });
  it('should mock bucket counters', () => {
    let bc = atlas.bucketCounter('bc', {
      function: 'latency',
      value: 3,
      unit: 's'
    });
    bc.record(4.5);
    assert(bc);
  });
  it('should mock bucket distribution summaries', () => {
    let bds = atlas.bucketDistSummary('bds', {
      function: 'decimal',
      value: 1000
    });
    bds.record(420.0);
    assert(bds);
  });
  it('should mock bucket timers', () => {
    let bt = atlas.bucketTimer('bt', {
      function: 'latencyBiasSlow',
      value: 5,
      unit: 'ms'
    });
    bt.record(0.002);
    assert(bt);
  });
  it('should mock start/stop', () => {
    atlas.start();
    atlas.stop();
  });
  it('should mock getDebugInfo/config/measurements', () => {
    atlas.getDebugInfo();
    atlas.config();
    atlas.measurements();
  });
  it('should mock push', () => {
    atlas.push([]);
  });
  it('should mock scope', () => {
    const a = atlas.scope({country: 'us'});
    a.counter('foo').increment();
    a.timer('foo').record([0, 42000]);
    const aa = a.scope({device: 'us'});
    aa.counter('errors').increment();
    assert(aa);
  });
});
