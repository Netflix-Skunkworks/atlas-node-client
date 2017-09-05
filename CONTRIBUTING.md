# Atlas Client for nodejs - Contributing guide

> Atlas Native Client for nodejs.

## Building

To compile the extension for the first time, run

```
$ npm i
$ npm run configure
$ npm run build
```

All subsequent builds only need `npm run build`

You can confirm everything built correctly by [running the test suite](#to-run-tests).

### Working With the Extension Locally

After building:

```node
$ node
> var atlas = require('./')
undefined
> atlas.counter("foo.bar").increment();
undefined
> atlas.counter("foo.bar").count();
1
```

### To run tests:

```
$ npm test
```

or to run test continuously

```
$ npm test -- watch
```

## The Parts

File | Contents
-------------|----------------
`atlas.cc` | Represents the top level of the module. C++ constructs that are exposed to javascript are exported here
`functions.cc` | top-level functions.
`index.js` | The main entry point for the node dependency
`binding.gyp` | Describes your node native extension to the build system (`node-gyp`). As you add source files to the project, you should also add them to the binding file.
