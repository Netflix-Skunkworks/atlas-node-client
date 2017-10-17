## 1.17.0 (2017-10-17)

#### Update

* libatlasclient (v3.4.0): control enabling/disabling of metrics using a file
  (used by the traffic team internally), plus some hardening efforts.
 
#### Fix

* `nodejs.gc.promotionRate` was sometimes negative.

#### Chore

* Travis setup
 
## 1.16.2 (2017-09-19)

* libatlasclient (v3.1.0): memory management improvements and better handling of large number of metrics
 
## 1.14.0 (2017-09-05)

#### Fix


* Update the usage of the `max_gauge` API. The 2.2.0 release of the
 native library is binary incompatible with previous versions due to a change
 of the `registry->max_gauge` return value. This call was broken in previous
 versions.
 
## 1.13.1 (2017-08-31)

#### Fix

* Latest libatlasclient (2.1.2) fixes default set of common tags

## 1.13.0 (2017-08-28)

#### Fix

* Latest libatlasclient (2.1.1) fixes the statistic count used by percentile timers

#### New

* If runtime metrics are enabled, the client will automatically track open and max file descriptors for the current process

## 1.12.0 (2017-08-10)

#### Update

* latest libatlasclient comes with a breaking API change but significant performance improvements

#### Fix

* Switch to a more accurate measure of the nodejs eventloop

## 1.11.1 (2017-08-02)

#### Update

* latest libatlasclient to get a bit more perf. when sending metrics

## 1.11.0 (2017-08-02)

#### Fix

* Depend on latest libatlasclient to get performance improvements related to sending metrics
* Match the Spectator tagging behavior for `intervalCounter`
 
## 1.10.0 (2017-07-31)

#### Fix

* Fix push functionality
 
## 1.10.0 (2017-06-09)

#### New

* Adds nf.task as a common tag (by depending on latest libatlasclient build)

## 1.9.2 (2017-04-24)

#### Fix

* Fix rpath on OSX ([b4339aa](https://stash.corp.netflix.com/projects/CLDMTA/repos/atlas-node-client/commits/b4339aa5871358b319765f95d7fe0f5843aa9ea9))

## 1.9.1 (2017-04-24)

#### Fix

* Default log dir is /logs/atlas ([0e44a1f](https://stash.corp.netflix.com/projects/CLDMTA/repos/atlas-node-client/commits/0e44a1fb45091a53eaf48014d6c7c5eb2d0f6a9f))
* Do not filter lag for the event loop ([d839bde](https://stash.corp.netflix.com/projects/CLDMTA/repos/atlas-node-client/commits/d839bde2ef8db709b00e6a5f6fa9219d06326a7c))
* Perform empty checks for names, keys, values ([823ebd2](https://stash.corp.netflix.com/projects/CLDMTA/repos/atlas-node-client/commits/823ebd236743cddc6f2b520bf84681b0da29b9b0))

#### Chore

* changelog tool now links to actual commits ([98a6351](https://stash.corp.netflix.com/projects/CLDMTA/repos/atlas-node-client/commits/98a63518093e120fa1ec581b32a825e61420eae5))

## 1.9.0 (2017-04-19)

#### Fix

* Refresh the liveDataSize to avoid expiration ([6d68565](https://stash.corp.netflix.com/projects/CLDMTA/repos/atlas-node-client/commits/6d68565d6483f258ada35da571c2a2dc03b3843d))

## 1.8.0 (2017-04-19)

#### New

* Event loop and GC metrics 

  * Timer: `nodejs.eventLoop` which measures the time it takes for the event loop to complete (using adaptive sampling)

  * Timer: `nodejs.eventLoopLag` which measures the lag nodejs has in executing a timer every 500ms. It should be 0, unless the nodejs process is too busy.

  * Timers: `nodejs.gc.pause` (`id=scavenge`, `id=incrementalMarking`, `id=markSweepCompact`, `id=processWeakCallbacks`), which measure the different v8 gc events

  * Counter: `nodejs.gc.allocationRate` measures in bytes/second the rate at which new memory is being allocated
   
  * Counter: `nodejs.gc.promotionRate` measures in bytes/second the rate at which memory is promoted to the old-space
   
  * Gauge: `nodejs.gc.liveDataSize` size in bytes of the old-space after a major GC event
   
  * Gauge: `nodejs.gc.maxDataSize` maximum size allowed for the heap

* Mocks

  * `require('atlasclient/mocks')`

## 1.7.0 
  * Provides a new config option to `atlas.start`: `atlas.start({logDirs: ['/foo/logs', '/bar/logs']})`
    default is: `['/logs', __dirname + '/logs']`. If we can't write to any directory passed, we write to /tmp.
    If we can't write to /tmp, then we log to stderr.
  * Add simple skeleton for mocks: `require('atlasclient/mocks')` - Will complete mocks in a subsequent release.

## 1.6.0
  * Provides `getDebugInfo` method

  * Provides `intervalCounter`: a counter that publishes its rate per second,
    plus the time since its last update

## 1.5.6
  *  Uses libatlasclient 1.5.2, which fixes shutdown issues

## 1.5.1 - 1.5.5
  *  Attempts to fix some compilation issues on platforms without pre-built
     binaries

## 1.5.0
  *  Fixes crash on timeout due to libcurl - back to node-pre-gyp method.

## 1.4.0
  *  Rollback to old compilation method due to a problem with the pre-packaged libcurl

## 1.3.0
  *  Installation via node-pre-gyp using a new artifactory repository for atlas-client

