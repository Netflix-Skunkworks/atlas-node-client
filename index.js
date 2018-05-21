'use strict';

let atlas = require('bindings')('atlas');
let updateInterval = 30000;
let nodeMetrics;

const path = require('path');

atlas.JsTimer.prototype.timeAsync = function(fun) {
  const self = this;
  const start = process.hrtime();
  const done = function() {
    self.record(process.hrtime(start));
  };
  return fun(done);
};

let ageGaugeUpdateValue = function(ageGauge) {
  let elapsed = ((new Date).getTime() - ageGauge.lastUpdated) / 1000.0;
  ageGauge.gauge.update(elapsed);
};

let AgeGauge = function(name, tags) {
  this.gauge = atlas.gauge(name, tags);
  this.lastUpdated = (new Date).getTime();
  this.gauge.update(0);
  setInterval(ageGaugeUpdateValue, updateInterval, this);
};

AgeGauge.prototype.value = function() {
  return this.gauge.value();
};

AgeGauge.prototype.update = function(updatedTime) {
  let t = updatedTime || (new Date).getTime();
  this.lastUpdated = t;
  ageGaugeUpdateValue(this);
};

let started = false;

function startAtlas(config) {
  if (started) {
    return;
  }

  const cfg = config ? config : {};
  // default log dirs
  let logDirs = ['/logs/atlas', path.join(__dirname, 'logs'), '/tmp'];

  if ('logDirs' in cfg) {
    logDirs = cfg.logDirs;
  }

  let runtimeMetrics = true;

  if ('runtimeMetrics' in cfg) {
    runtimeMetrics = cfg.runtimeMetrics;
  }

  let developmentMode = false;

  if ('developmentMode' in cfg) {
    developmentMode = cfg.developmentMode;
  }

  const nodeVersion = {
    'nodejs.version': process.version
  };
  atlas.start({
    developmentMode: developmentMode,
    logDirs: logDirs,
    runtimeMetrics: runtimeMetrics,
    runtimeTags: nodeVersion
  });

  if (runtimeMetrics) {
    const nm = require('./node-metrics');
    nodeMetrics = new nm.NodeMetrics(atlas, nodeVersion);
    nodeMetrics.start();
  }

  started = true;
}

function stopAtlas() {
  if (!started) {
    return;
  }

  if (nodeMetrics) {
    nodeMetrics.stop();
    nodeMetrics = undefined;
  }
  atlas.stop();
  started = false;
}

function debugInfo() {
  return {
    config: atlas.config(),
    measurements: atlas.measurements()
  };
}

function devMode(enabled) {
  atlas.setDevMode(enabled);
}

function analyzeId(name, tags) {
  return atlas.analyzeId(name, tags);
}

let scope = function(commonTags) {
  let bucketArgs = function(name) {
    let tags, bucketFunction;

    if (arguments.length === 3) {
      tags = arguments[1];
      bucketFunction = arguments[2];
    } else {
      tags = {};
      bucketFunction = arguments[1];
    }

    return [name, Object.assign({}, commonTags, tags), bucketFunction];
  };

  let s = {
    start: startAtlas,
    stop: stopAtlas,
    setDevMode: devMode,
    getDebugInfo: debugInfo,
    analyzeId: analyzeId,
    counter: (name, tags) => atlas.counter(
      name, Object.assign({}, commonTags, tags)),
    dcounter: (name, tags) => atlas.dcounter(
      name, Object.assign({}, commonTags, tags)),
    intervalCounter: (name, tags) => atlas.intervalCounter(
      name, Object.assign({}, commonTags, tags)),
    timer: (name, tags) => atlas.timer(
      name, Object.assign({}, commonTags, tags)),
    gauge: (name, tags) => atlas.gauge(
      name, Object.assign({}, commonTags, tags)),
    maxGauge: (name, tags) => atlas.maxGauge(
      name, Object.assign({}, commonTags, tags)),
    distSummary: (name, tags) => atlas.distSummary(
      name, Object.assign({}, commonTags, tags)),
    longTaskTimer: (name, tags) => atlas.longTaskTimer(
      name, Object.assign({}, commonTags, tags)),
    percentileDistSummary: (name, tags) => atlas.percentileDistSummary(
      name, Object.assign({}, commonTags, tags)),
    percentileTimer: (name, tags) => atlas.percentileTimer(
      name, Object.assign({}, commonTags, tags)),
    age: (name, tags) => new AgeGauge(
      name, Object.assign({}, commonTags, tags)),
    bucketCounter: function() {
      let args = bucketArgs.apply(this, arguments);
      return atlas.bucketCounter(args[0], args[1], args[2]);
    },
    bucketDistSummary: function() {
      let args = bucketArgs.apply(this, arguments);
      return atlas.bucketDistSummary(args[0], args[1], args[2]);
    },
    bucketTimer: function() {
      let args = bucketArgs.apply(this, arguments);
      return atlas.bucketTimer(args[0], args[1], args[2]);
    },
    measurements: () => atlas.measurements(),
    // used by the old prana interface. Should not be used by new code.
    // sends metrics immediately to atlas, they're not available for alerts/cloudwatch/etc.
    push: (metrics) => atlas.push(metrics),
    config: () => atlas.config(),
    scope: tags => scope(Object.assign({}, commonTags, tags)),
    // for testing
    setUpdateInterval: function(newInterval) {
      updateInterval = newInterval;
    }
  };

  return s;
};

module.exports = scope({});
