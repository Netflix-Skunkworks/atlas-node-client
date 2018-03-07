#pragma once

#include <atlas/atlas_client.h>
#include <atlas/meter/bucket_counter.h>
#include <atlas/meter/bucket_distribution_summary.h>
#include <atlas/meter/bucket_timer.h>
#include <atlas/meter/counter.h>
#include <atlas/meter/interval_counter.h>
#include <atlas/meter/percentile_dist_summary.h>
#include <atlas/meter/percentile_timer.h>
#include <atlas/meter/subscription_distribution_summary.h>
#include <atlas/meter/subscription_gauge.h>
#include <atlas/meter/subscription_max_gauge.h>
#include <nan.h>

// enable/disable development mode
NAN_METHOD(set_dev_mode);

// get an array of measurements intended for the main publish pipeline
NAN_METHOD(measurements);
//
// get the current config
NAN_METHOD(config);

// push a list of measurements immediately to atlas
NAN_METHOD(push);

// get a counter
NAN_METHOD(counter);

// get a double counter
NAN_METHOD(dcounter);

// get an interval counter
NAN_METHOD(interval_counter);

// get a timer
NAN_METHOD(timer);

// get a long task timer
NAN_METHOD(long_task_timer);

// get a gauge
NAN_METHOD(gauge);

// get a max gauge
NAN_METHOD(max_gauge);

// get a distribution summary
NAN_METHOD(dist_summary);

// bucket counter
NAN_METHOD(bucket_counter);

// bucket distribution summary
NAN_METHOD(bucket_dist_summary);

// bucket timer
NAN_METHOD(bucket_timer);

// percentile timer
NAN_METHOD(percentile_timer);

// bucket timer
NAN_METHOD(percentile_dist_summary);

// wrapper for a counter
class JsCounter : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsCounter(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Increment);
  static NAN_METHOD(Add);
  static NAN_METHOD(Count);

  std::shared_ptr<atlas::meter::Counter> counter_;
};

class JsDCounter : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsDCounter(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Increment);
  static NAN_METHOD(Add);
  static NAN_METHOD(Count);

  std::shared_ptr<atlas::meter::DCounter> counter_;
};

// wrapper for a timer
class JsTimer : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsTimer(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Record);
  static NAN_METHOD(TimeThis);
  static NAN_METHOD(TotalTime);
  static NAN_METHOD(Count);

  std::shared_ptr<atlas::meter::Timer> timer_;
};

// wrapper for a long task timer
class JsLongTaskTimer : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsLongTaskTimer(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Start);
  static NAN_METHOD(Stop);
  static NAN_METHOD(Duration);
  static NAN_METHOD(ActiveTasks);

  std::shared_ptr<atlas::meter::LongTaskTimer> timer_;
};

// wrapper for a gauge
class JsGauge : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  JsGauge(atlas::meter::IdPtr id);
  static NAN_METHOD(New);
  static NAN_METHOD(Value);
  static NAN_METHOD(Update);

  std::shared_ptr<atlas::meter::Gauge<double>> gauge_;
};

// wrapper for a max gauge
class JsMaxGauge : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsMaxGauge(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Update);
  static NAN_METHOD(Value);

  std::shared_ptr<atlas::meter::Gauge<double>> max_gauge_;
};

class JsDistSummary : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsDistSummary(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Record);
  static NAN_METHOD(TotalAmount);
  static NAN_METHOD(Count);

  std::shared_ptr<atlas::meter::DistributionSummary> dist_summary_;
};

class JsBucketCounter : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsBucketCounter(atlas::meter::IdPtr id,
                           atlas::meter::BucketFunction bucket_function);

  static NAN_METHOD(New);
  static NAN_METHOD(Record);

  std::shared_ptr<atlas::meter::BucketCounter> bucket_counter_;
};

class JsBucketDistSummary : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsBucketDistSummary(atlas::meter::IdPtr id,
                               atlas::meter::BucketFunction bucket_function);

  static NAN_METHOD(New);
  static NAN_METHOD(Record);

  std::shared_ptr<atlas::meter::BucketDistributionSummary> bucket_dist_summary_;
};

class JsBucketTimer : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsBucketTimer(atlas::meter::IdPtr id,
                         atlas::meter::BucketFunction bucket_function);

  static NAN_METHOD(New);
  static NAN_METHOD(Record);

  std::shared_ptr<atlas::meter::BucketTimer> bucket_timer_;
};

class JsPercentileTimer : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsPercentileTimer(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Record);
  static NAN_METHOD(Percentile);
  static NAN_METHOD(TotalTime);
  static NAN_METHOD(Count);

  std::shared_ptr<atlas::meter::PercentileTimer> perc_timer_;
};

class JsPercentileDistSummary : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsPercentileDistSummary(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Record);
  static NAN_METHOD(Percentile);
  static NAN_METHOD(TotalAmount);
  static NAN_METHOD(Count);

  std::shared_ptr<atlas::meter::PercentileDistributionSummary>
      perc_dist_summary_;
};

class JsIntervalCounter : public Nan::ObjectWrap {
 public:
  static NAN_MODULE_INIT(Init);
  static Nan::Persistent<v8::Function> constructor;

 private:
  explicit JsIntervalCounter(atlas::meter::IdPtr id);

  static NAN_METHOD(New);
  static NAN_METHOD(Increment);
  static NAN_METHOD(Add);
  static NAN_METHOD(Count);
  static NAN_METHOD(SecondsSinceLastUpdate);

  std::shared_ptr<atlas::meter::IntervalCounter> counter_;
};
