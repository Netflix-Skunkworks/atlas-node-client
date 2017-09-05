'use strict';

const v8 = require('v8');

function NodeMetrics(atlas, extraTags) {
  this.atlas = atlas;
  this.rss = atlas.gauge('nodejs.rss', extraTags);
  this.heapTotal = atlas.gauge('nodejs.heapTotal', extraTags);
  this.heapUsed = atlas.gauge('nodejs.heapUsed', extraTags);
  this.external = atlas.gauge('nodejs.external', extraTags);
  this.extraTags = extraTags;
  this.running = false;

  if (typeof process.cpuUsage === 'function') {
    this.cpuUsageUser = atlas.gauge('nodejs.cpuUsage', {
      id: 'user',
      'nodejs.version': process.version
    });
    this.cpuUsageSystem = atlas.gauge('nodejs.cpuUsage', {
      id: 'system',
      'nodejs.version': process.version
    });
    this.lastCpuUsage = process.cpuUsage();
    this.lastCpuUsageTime = process.hrtime();
  }
}
module.exports.NodeMetrics = NodeMetrics;

function measureEventLoopTime(self) {
  setImmediate(function() {
    const start = process.hrtime();
    setImmediate(function() {
      self.eventLoopTime.record(process.hrtime(start));
    });
  });
}

module.exports._measureEventLoopTime = measureEventLoopTime;

NodeMetrics.prototype.start = function(refreshFreq) {
  this.running = true;
  const refresh = refreshFreq ? refreshFreq : 30000;
  this.intervalId = setInterval(refreshValues, refresh, this);
  this.eventLoopTime = this.atlas.timer('nodejs.eventLoop', this.extraTags);
  this.eventLoopFreq = 500;
  this.eventLoopInterval = setInterval(measureEventLoopTime,
    this.eventLoopFreq, this);
  measureEventLoopTime(this);
};

NodeMetrics.prototype.stop = function() {
  this.running = false;

  if (this.intervalId) {
    clearInterval(this.intervalId);
    this.intervalId = undefined;
  }

  if (this.eventLoopImmediate) {
    clearImmediate(this.eventLoopImmediate);
    this.eventLoopImmediate = undefined;
  }

  if (this.eventLoopInterval) {
    clearInterval(this.eventLoopInterval);
    this.eventLoopInterval = undefined;
  }
};

function deltaMicros(end, start) {
  let deltaNanos = end[1] - start[1];
  let deltaSecs = end[0] - start[0];

  if (deltaNanos < 0) {
    deltaNanos += 1000000000;
    deltaSecs -= 1;
  }
  return Math.trunc(deltaSecs * 1e6 + deltaNanos / 1e3);
}

function updatePercentage(gauge, currentUsed, prevUsed, totalMicros) {
  const delta = currentUsed - prevUsed;
  const percentage = delta / totalMicros * 100.0;
  gauge.update(percentage);
}

function toCamelCase(s) {
  return s.replace(/_([a-z])/g, function(g) {
    return g[1].toUpperCase();
  });
}

function updateV8HeapGauges(atlas, extraTags, heapStats) {
  for (const key of Object.keys(heapStats)) {
    const name = 'nodejs.' + toCamelCase(key);
    atlas.gauge(name, extraTags).update(heapStats[key]);
  }
}

function updateV8HeapSpaceGauges(atlas, extraTags, heapSpaceStats) {
  for (const space of heapSpaceStats) {
    const id = toCamelCase(space.space_name);

    for (const key of Object.keys(space)) {

      if (key !== 'space_name') {
        const name = 'nodejs.' + toCamelCase(key);
        const tags = Object.assign({
          id: id
        }, extraTags);
        atlas.gauge(name, tags).update(space[key]);
      }
    }
  }
}

function refreshValues(self) {
  const memUsage = process.memoryUsage();
  self.rss.update(memUsage.rss);
  self.heapTotal.update(memUsage.heapTotal);
  self.heapUsed.update(memUsage.heapUsed);
  self.external.update(memUsage.external);

  if (typeof process.cpuUsage === 'function') {
    const newCpuUsage = process.cpuUsage();
    const newCpuUsageTime = process.hrtime();

    const elapsedMicros = deltaMicros(newCpuUsageTime, self.lastCpuUsageTime);
    updatePercentage(self.cpuUsageUser, newCpuUsage.user,
      self.lastCpuUsage.user, elapsedMicros);
    updatePercentage(self.cpuUsageSystem, newCpuUsage.system,
      self.lastCpuUsage.system, elapsedMicros);

    self.lastCpuUsageTime = newCpuUsageTime;
    self.lastCpuUsage = newCpuUsage;
  }

  updateV8HeapGauges(self.atlas, self.extraTags, v8.getHeapStatistics());

  if (typeof v8.getHeapSpaceStatistics === 'function') {
    updateV8HeapSpaceGauges(self.atlas, self.extraTags,
      v8.getHeapSpaceStatistics());
  }
}
