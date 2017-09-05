'use strict';

const sinon = require('sinon');

function atlas() {
  // mock atlas
  const counter = sinon.stub();
  const counterIncrement = sinon.spy();
  const intervalCounter = sinon.stub();
  const intervalCounterIncrement = sinon.spy();
  const timer = sinon.stub();
  const timerRecord = sinon.spy();
  const maxGauge = sinon.stub();
  const maxGaugeUpdate = sinon.spy();
  const gauge = sinon.stub();
  const gaugeUpdate = sinon.spy();
  const distSummary = sinon.stub();
  const distRecord = sinon.spy();
  const longTaskTimer = sinon.stub();
  const longTaskTimerRecord = sinon.spy();
  const percentileDistSummary = sinon.stub();
  const percentileDistSummaryRecord = sinon.spy();
  const percentileTimer = sinon.stub();
  const percentileTimerRecord = sinon.spy();
  const age = sinon.stub();
  const ageUpdate = sinon.spy();
  const bucketCounter = sinon.stub();
  const bucketCounterRecord = sinon.spy();
  const bucketDistSummary = sinon.stub();
  const bucketDistSummaryRecord = sinon.spy();
  const bucketTimer = sinon.stub();
  const bucketTimerRecord = sinon.spy();
  const getDebugInfo = sinon.spy();
  const start = sinon.spy();
  const stop = sinon.spy();
  const config = sinon.spy();
  const measurements = sinon.spy();
  const push = sinon.spy();
  const apiExceptScope = {
    counter: counter.returns({
      increment: counterIncrement
    }),
    intervalCounter: intervalCounter.returns({
      increment: intervalCounterIncrement
    }),
    timer: timer.returns({
      record: timerRecord
    }),
    gauge: gauge.returns({
      update: gaugeUpdate
    }),
    maxGauge: maxGauge.returns({
      update: maxGaugeUpdate
    }),
    distSummary: distSummary.returns({
      record: distRecord
    }),
    longTaskTimer: longTaskTimer.returns({
      record: longTaskTimerRecord
    }),
    percentileDistSummary: percentileDistSummary.returns({
      record: percentileDistSummaryRecord
    }),
    percentileTimer: percentileTimer.returns({
      record: percentileTimerRecord
    }),
    age: age.returns({
      update: ageUpdate
    }),
    bucketCounter: bucketCounter.returns({
      record: bucketCounterRecord
    }),
    bucketDistSummary: bucketDistSummary.returns({
      record: bucketDistSummaryRecord
    }),
    bucketTimer: bucketTimer.returns({
      record: bucketTimerRecord
    }),
    getDebugInfo: getDebugInfo,
    start: start,
    stop: stop,
    config: config,
    measurements: measurements,
    push: push
  };
  const scope = sinon.stub();
  scope.returns(Object.assign({}, apiExceptScope, {scope: scope}));
  const stubs = {
      counter: counter,
      counterIncrement: counterIncrement,
      intervalCounter: intervalCounter,
      intervalCounterIncrement: intervalCounterIncrement,
      timer: timer,
      timerRecord: timerRecord,
      gauge: gauge,
      gaugeUpdate: gaugeUpdate,
      maxGauge: maxGauge,
      maxGaugeUpdate: maxGaugeUpdate,
      distSummary: distSummary,
      distSummaryRecord: distRecord,
      longTaskTimer: longTaskTimer,
      longTaskTimerRecord: longTaskTimerRecord,
      percentileDistSummary: percentileDistSummary,
      percentileDistSummaryRecord: percentileDistSummaryRecord,
      percentileTimer: percentileTimer,
      percentileTimerRecord: percentileTimerRecord,
      age: age,
      ageUpdate: ageUpdate,
      bucketCounter: bucketCounter,
      bucketCounterRecord: bucketCounterRecord,
      bucketDistSummary: bucketDistSummary,
      bucketDistSummaryRecord: bucketDistSummaryRecord,
      bucketTimer: bucketTimer,
      bucketTimerRecord: bucketTimerRecord,
      getDebugInfo: getDebugInfo,
      start: start,
      stop: stop,
      config: config,
      scope: scope
    };

  const mocks = Object.assign({}, apiExceptScope, stubs);
  return mocks;
}

module.exports = atlas();
