# Gauges

A gauge is a handle to get the current value. Typical examples for gauges
would be the size of a queue or the amount of heap memory used by an
application.
Since gauges are sampled, there is no information about what might have
occurred between samples.

Consider monitoring the behavior of a queue of tasks. If the data is being
collected once a minute, then a gauge for the size will show the size when
it was sampled. The size may have been much higher or lower at some point
during interval, but that is not known.

## Example

Let's create a simple module that will monitor the memory utilization of the nodejs process. We'll keep track of the `rss`, `heapTotal`, `heapUsed`, and `external` values as reported by `process.memoryUsage`:

```js
function NodeMetrics(atlas) {
  this.atlas = atlas;
  this.rss = atlas.gauge('nodejs.rss');
  this.heapTotal = atlas.gauge('nodejs.heapTotal');
  this.heapUsed = atlas.gauge('nodejs.heapUsed');
  this.external = atlas.gauge('nodejs.external');
}

module.exports.NodeMetrics = NodeMetrics;

NodeMetrics.prototype.start = function(refreshFreq) {
  let refresh = refreshFreq ? refreshFreq : 30000;
  this.intervalId = setInterval(refreshValues, refresh, this);
}

function refreshValues(self) {
  const memUsage = process.memoryUsage();
  self.rss.update(memUsage.rss);
  self.heapTotal.update(memUsage.heapTotal);
  self.heapUsed.update(memUsage.heapUsed);
  self.external.update(memUsage.external);
}
```

Note that we update the value periodically. The last value set before the metrics are sent to atlas is the value that we will observe in our graphs.

We can also query the values programmatically:

```js
> atlas.gauge('nodejs.rss').value()
26742784
> atlas.gauge('nodejs.rss').value()
27095040
```
