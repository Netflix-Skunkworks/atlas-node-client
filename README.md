# Atlas Client for Node.js

[![Snapshot](https://github.com/Netflix-Skunkworks/atlas-node-client/actions/workflows/snapshot.yml/badge.svg)](https://github.com/Netflix-Skunkworks/atlas-node-client/actions/workflows/snapshot.yml)

> :warning: This library is deprecated. Javascript projects should migrate to
[spectator-js](https://github.com/Netflix/spectator-js) for publishing Atlas
metrics.

Module for generating metrics and sending them to Atlas.

## Getting Started

Install the module with: `npm install atlasclient`

## Usage

```js
const atlas = require("atlasclient");
atlas.start(); // only the main application, not libraries instrumenting code

//...
app.get("/somehandler", (req, res) => {
  let start = process.hrtime();
  // ...
  // do the work
  // statusCode is set here
  let elapsed = process.hrtime(start);
  atlas.timer('server.requestLatency').record(elapsed);
  atlas.counter('server.requestCount', {
    country: getCountry(req),
    status: statusCode
  }).increment();

  let responseSize = getResponseSize(res);
  atlas.distSummary('server.responseSize').record(responseSize);
});

// only for the main application, not libraries
process.on('beforeExit', () => {
  atlas.stop(); // FIXME
});
```

## Starting and Stopping

In your main application call `atlas.start()`.

If you wish to opt-out of [Node.js runtime metrics](doc/nodejs-metrics.md), pass `{runtimeMetrics: false}` to the start method.

## Instrumenting Code

See the usage guides for [counters](doc/counter.md), [timers](doc/timer.md), [gauges](doc/gauge.md),
and [distribution summaries](doc/dist-summary.md).

In addition to the basic meters, you can also choose to use more expensive meters that allow you to
get an idea of the distribution of samples:

* [buckets](doc/buckets.md)
* [percentiles](doc/percentiles.md)

## Unit Testing

See the [test] directory for examples of unit testing.  These tests can be run with `npm test`.

[test]: https://stash.corp.netflix.com/projects/CLDMTA/repos/atlas-node-client/browse/test

## Development Mode

When developing it's sometimes useful to throw errors when constructing invalid
metrics. By default the client will ignore these errors so it doesn't affect a
running application in production, but you can change this behavior by enabling
the development mode:

`atlas.setDevMode(true);`

This will validate metrics can be successfully sent to atlas, and throw in case of errors.

## Debugging

* Configuration for the atlas-native-client, the dependency of the atlas-node-client that is responsible
for publishing metrics to the Atlas backend, is centrally controlled, but can be overridden with the file
`/usr/local/etc/atlas-config.json`.
* Enable verbose logging in atlas-native-client by adding `{ "logVerbosity": 1 }` to `atlas-config.json`.
Possible log file directories are listed below; the first that can be written to is used.  The generated
directory path is typically `$PWD/node_modules/atlasclient/logs`.
    ```js
    let logDirs = ['/logs/atlas', path.join(__dirname, 'logs'), '/tmp'];
    ```
* The `validateMetrics` option in atlas-native-client defaults to true.  It can be overridden by
adding `"validateMetrics": false` to the `atlas-config.json` file.
* `config()` Dump the internal configuration of the atlasclient package. The `commonTags` are
extracted from environment variables.  The `dumpMetrics` option controls whether or not the exact
payload sent to the Atlas backend is logged.

    ```js
    { evaluateUrl: 'http://atlas-lwcapi-iep.us-east-1.ieptest.netflix.net/lwc/api/v1/evaluate',
      subscriptionsUrl: 'http://atlas-lwcapi-iep.us-east-1.ieptest.netflix.net/lwc/api/v1/expressions/go2-client',
      publishUrl: 'http://atlas-pub-179727101194.us-east-1.ieptest.netflix.net/api/v1/publish-fast',
      subscriptionsRefreshMillis: 10000,
      batchSize: 10000,
      connectTimeout: 1,
      readTimeout: 10,
      dumpMetrics: false,
      dumpSubscriptions: false,
      publishEnabled: true,
      publishConfig: [ ':true,:all' ],
      commonTags:
       { 'nf.account': '179727101194',
         'nf.ami': 'ami-be748aa8',
         'nf.app': 'go2',
         'nf.asg': 'go2-client-v012',
         'nf.cluster': 'go2-client',
         'nf.node': 'i-0feaad16fc792bca6',
         'nf.region': 'us-east-1',
         'nf.vmtype': 'm3.medium',
         'nf.zone': 'us-east-1c' } }
    ```

* `measurements()`  This can be used to get a list of the current measurements.  The return result
is an array of objects with two properties: `tags` and `value`.

    ```js
    [ { tags:
         { name: 'atlas.client.mainMeasurements',
           'nf.account': '179727101194',
           'nf.ami': 'ami-be748aa8',
           'nf.app': 'go2',
           'nf.asg': 'go2-client-v012',
           'nf.cluster': 'go2-client',
           'nf.node': 'i-0feaad16fc792bca6',
           'nf.region': 'us-east-1',
           'nf.vmtype': 'm3.medium',
           'nf.zone': 'us-east-1c' },
        value: 3 },
      { tags:
         { name: 'atlas.client.meters',
           'nf.account': '179727101194',
           'nf.ami': 'ami-be748aa8',
           'nf.app': 'go2',
           'nf.asg': 'go2-client-v012',
           'nf.cluster': 'go2-client',
           'nf.node': 'i-0feaad16fc792bca6',
           'nf.region': 'us-east-1',
           'nf.vmtype': 'm3.medium',
           'nf.zone': 'us-east-1c' },
        value: 3 },
      { tags:
         { name: 'atlas.client.rawMainMeasurements',
           'nf.account': '179727101194',
           'nf.ami': 'ami-be748aa8',
           'nf.app': 'go2',
           'nf.asg': 'go2-client-v012',
           'nf.cluster': 'go2-client',
           'nf.node': 'i-0feaad16fc792bca6',
           'nf.region': 'us-east-1',
           'nf.vmtype': 'm3.medium',
           'nf.zone': 'us-east-1c' },
        value: 3 } ]
    ```

## Internal

* `push(measurements)`

Send the array of measurements immediately to the server, without adding any common tags.

A measurement is an object with three properties:

* `start`: timestamp in milliseconds
* `tags`: object describing how to tag the measurement. `name` is required.
* `value`: number

## Help, this doesn't work on Node 8 with Babel?

If you receive an error like:

```
/Users/sdhillon/projects/quitelite/node_modules/core-js/modules/_typed-buffer.js:157
  if(numberLength != byteLength)throw RangeError(WRONG_LENGTH);
                                ^

RangeError: Wrong length!
    at validateArrayBufferArguments (/Users/sdhillon/projects/quitelite/node_modules/core-js/modules/_typed-buffer.js:157:39)
    at new ArrayBuffer (/Users/sdhillon/projects/quitelite/node_modules/core-js/modules/_typed-buffer.js:247:29)
    at v8.js:132:17
    at NativeModule.compile (bootstrap_node.js:563:7)
    at Function.NativeModule.require (bootstrap_node.js:506:18)
    at Function.Module._load (module.js:446:25)
    at Module.require (module.js:513:17)
    at require (internal/module.js:11:18)
    at repl:1:-61
    at ContextifyScript.Script.runInThisContext (vm.js:44:33)
```

On Node 8, core-js does not properly support array buffer construction. Since we pull
metrics from v8, and v8 relys on `ArrayBuffer`, this does not work at the moment. Fortunately,
when starting Atlas, you can disable this if you're hitting this issue. `start(...)` takes
the following configuration options:

```
{
    runtimeMetrics: boolean, // default: true
    logDirs: Array<string>, // default: ['/logs/atlas', path.join(__dirname, 'logs'), '/tmp'];   
}
```

If you disable runtimeMetrics, atlas client will not load v8, and therefore
it will not automatically generate metrics like:

* nodejs.rss
* nodejs.heapTotal
* nodejs.heapUsed
* nodejs.external

