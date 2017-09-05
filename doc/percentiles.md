# Percentile Meters

## Percentile Timer and Distribution Summaries
```js

const timer = atlas.percentileTimer('requestLatency');
const distSummary = atlas.percentileDistSummary('responseBytes');
const start = process.hrtime();
//...

timer.record(process.hrtime(start));
distSummary.record(getResponseSize(response));


// read api: compute a local (approximate) percentile
// timers report times in seconds
const median = timer.percentile(50);
const tp95 = timer.percentile(95);
const tp99 = timer.percentile(99);

const dMedian = distSummary.percentile(50);

// you can graph the meters using the :percentile atlas stack language operator
// For example:
//    name,requestLatency,:eq,(,25,50,90,),:percentiles
// see: https://github.com/Netflix/atlas/wiki/math-percentiles
```
