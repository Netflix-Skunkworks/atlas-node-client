# Counters

A counter is used to measure the rate at which some event is occurring.
Consider a simple stack, counters would be used to measure things like the
rate at which items are being pushed and popped.

Counters are created using the atlas registry which will be setup as part of
application initialization. For example:

```js
function Stack(atlas) {
  this.size = 0;
  //...
  this.pushCounter = atlas.counter('pushCounter');
  this.popCounter = atlas.counter('popCounter');
}

Stack.prototype.push = function (item) {
  //...
  this.pushCounter.increment();
};

Stack.prototype.pop = function () {
  //...
  this.popCounter.increment();
};
```

Optionally an amount can be passed in when calling `increment`. This is useful
when a collection of events happens together.

```js
Stack.prototype.pushAll = function (items) {
  // push each item in items
  //...
  this.pushCounter.increment(items.length);
};
```

In addition to `increment` counters also provide a `count` method that returns the
total number of events that have occurred since the counter was created.
