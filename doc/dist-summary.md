# Distribution Summaries

A distribution summary is used to track the distribution of events. It is
similar to a timer, but more general in that the size does not have to be
a period of time. For example, a distribution summary could be used to measure
the payload sizes of requests hitting a server.

It is recommended to always use base units when recording the data. So if
measuring the payload size use bytes, not kilobytes or some other unit.

To get started create an instance of a Distribution Summary:

```js
function Server(atlas) {
  this.requestSize = atlas.distSummary('server.requestSize');
}
```

Then call `record` when an event occurs:

```js
Server.prototype.handle = function (req) {
  this.requestSize.record(getReqSizeInBytes(req));
}
```
