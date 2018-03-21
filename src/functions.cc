#include "functions.h"
#include "utils.h"
#include <chrono>
#include <sstream>

using atlas::meter::BucketFunction;
using atlas::meter::IdPtr;
using atlas::meter::Measurement;
using atlas::meter::Measurements;
using atlas::meter::Tags;
using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Maybe;
using v8::Object;

NAN_METHOD(set_dev_mode) {
  if (info.Length() == 1 && info[0]->IsBoolean()) {
    auto b = info[0]->BooleanValue();
    dev_mode = b;
  }
}

NAN_METHOD(measurements) {
  auto config = atlas::GetConfig();
  auto common_tags = config.CommonTags();
  const auto& measurements =
      atlas_registry.GetMainMeasurements(config, common_tags);

  auto ret = Nan::New<v8::Array>();

  for (const auto& m : *measurements) {
    auto measurement = Nan::New<Object>();
    auto tags = Nan::New<Object>();
    const auto& t = m->all_tags();
    for (const auto& kv : t) {
      tags->Set(Nan::New(kv.first.get()).ToLocalChecked(),
                Nan::New(kv.second.get()).ToLocalChecked());
    }
    measurement->Set(Nan::New("tags").ToLocalChecked(), tags);
    measurement->Set(Nan::New("value").ToLocalChecked(), Nan::New(m->value()));
    ret->Set(ret->Length(), measurement);
  }

  info.GetReturnValue().Set(ret);
}

NAN_METHOD(config) {
  auto currentCfg = atlas::GetConfig();
  const auto& endpoints = currentCfg.EndpointConfiguration();
  const auto& log = currentCfg.LogConfiguration();
  const auto& http = currentCfg.HttpConfiguration();
  auto ret = Nan::New<Object>();
  ret->Set(Nan::New("evaluateUrl").ToLocalChecked(),
           Nan::New(endpoints.evaluate.c_str()).ToLocalChecked());
  ret->Set(Nan::New("subscriptionsUrl").ToLocalChecked(),
           Nan::New(endpoints.subscriptions.c_str()).ToLocalChecked());
  ret->Set(Nan::New("publishUrl").ToLocalChecked(),
           Nan::New(endpoints.publish.c_str()).ToLocalChecked());
  ret->Set(Nan::New("subscriptionsRefreshMillis").ToLocalChecked(),
           Nan::New(static_cast<double>(currentCfg.SubRefreshMillis())));
  ret->Set(Nan::New("batchSize").ToLocalChecked(), Nan::New(http.batch_size));
  ret->Set(Nan::New("connectTimeout").ToLocalChecked(),
           Nan::New(http.connect_timeout));
  ret->Set(Nan::New("readTimeout").ToLocalChecked(),
           Nan::New(http.read_timeout));
  ret->Set(Nan::New("dumpMetrics").ToLocalChecked(),
           Nan::New(log.dump_metrics));
  ret->Set(Nan::New("dumpSubscriptions").ToLocalChecked(),
           Nan::New(log.dump_subscriptions));
  ret->Set(Nan::New("publishEnabled").ToLocalChecked(),
           Nan::New(currentCfg.IsMainEnabled()));

  auto pubCfg = Nan::New<v8::Array>();
  for (const auto& expr : currentCfg.PublishConfig()) {
    pubCfg->Set(pubCfg->Length(), Nan::New(expr.c_str()).ToLocalChecked());
  }
  ret->Set(Nan::New("publishConfig").ToLocalChecked(), pubCfg);
  auto commonTags = Nan::New<Object>();
  for (const auto& kv : currentCfg.CommonTags()) {
    commonTags->Set(Nan::New(kv.first.get()).ToLocalChecked(),
                    Nan::New(kv.second.get()).ToLocalChecked());
  }
  ret->Set(Nan::New("commonTags").ToLocalChecked(), commonTags);
  ret->Set(Nan::New("loggingDirectory").ToLocalChecked(),
           Nan::New(currentCfg.LoggingDirectory().c_str()).ToLocalChecked());

  info.GetReturnValue().Set(ret);
}

static Measurement getMeasurement(v8::Isolate* isolate, Local<v8::Value> v) {
  std::string err_msg;
  auto o = v->ToObject();
  auto nameVal =
      Nan::Get(o, Nan::New("name").ToLocalChecked()).ToLocalChecked();
  Nan::Utf8String utf8(nameVal);
  std::string name(*utf8);
  auto timestamp = Nan::Get(o, Nan::New("timestamp").ToLocalChecked())
                       .ToLocalChecked()
                       ->IntegerValue();
  auto value = Nan::Get(o, Nan::New("value").ToLocalChecked())
                   .ToLocalChecked()
                   ->NumberValue();
  auto tagsObj = Nan::Get(o, Nan::New("tags").ToLocalChecked())
                     .ToLocalChecked()
                     ->ToObject();
  Tags tags;
  tagsFromObject(isolate, tagsObj, &tags, &err_msg);

  auto id = atlas_registry.CreateId(name, tags);
  return Measurement{id, timestamp, value};
}

NAN_METHOD(push) {
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto measurements = info[0].As<v8::Array>();
    Measurements ms;
    for (uint32_t i = 0; i < measurements->Length(); ++i) {
      ms.push_back(getMeasurement(info.GetIsolate(), measurements->Get(i)));
    }
    PushMeasurements(ms);
  } else {
    Nan::ThrowError("atlas.push() expects an array of measurements");
  }
}

constexpr int kMaxArgs = 3;

static void CreateConstructor(const Nan::FunctionCallbackInfo<v8::Value>& info,
                              Nan::Persistent<Function>& constructor,
                              const char* name) {
  const int argc = info.Length();
  if (argc > kMaxArgs) {
    std::ostringstream os;
    os << "Too many arguments to " << name << " (" << argc
       << " received, max is " << kMaxArgs << ")";
    Nan::ThrowError(os.str().c_str());
    return;
  }
  if (argc == 0 || info[0]->IsUndefined()) {
    Nan::ThrowError("Need at least a name argument");
    return;
  }
  Local<v8::Value> argv[kMaxArgs];
  for (auto i = 0; i < argc; ++i) {
    argv[i] = info[i];
  }

  auto cons = Nan::New<Function>(constructor);
  auto isolate = info.GetIsolate();
  auto context = isolate->GetCurrentContext();
  Nan::TryCatch tc;
  auto newInstance = cons->NewInstance(context, argc, argv);
  if (newInstance.IsEmpty()) {
    if (tc.HasCaught()) {
      tc.ReThrow();
    } else {
      Nan::ThrowError("Invalid arguments to constructor. Cannot create meter.");
    }
  } else {
    info.GetReturnValue().Set(newInstance.ToLocalChecked());
  }
}

NAN_METHOD(counter) {
  CreateConstructor(info, JsCounter::constructor, "counter");
}

NAN_METHOD(dcounter) {
  CreateConstructor(info, JsDCounter::constructor, "dcounter");
}

NAN_METHOD(interval_counter) {
  CreateConstructor(info, JsIntervalCounter::constructor, "intervalCounter");
}

NAN_METHOD(max_gauge) {
  CreateConstructor(info, JsMaxGauge::constructor, "maxGauge");
}

NAN_METHOD(dist_summary) {
  CreateConstructor(info, JsDistSummary::constructor, "distSummary");
}

NAN_METHOD(bucket_counter) {
  CreateConstructor(info, JsBucketCounter::constructor, "bucketCounter");
}

NAN_METHOD(bucket_dist_summary) {
  CreateConstructor(info, JsBucketDistSummary::constructor,
                    "bucketDistributionSummary");
}

NAN_METHOD(bucket_timer) {
  CreateConstructor(info, JsBucketTimer::constructor, "bucketTimer");
}

NAN_METHOD(percentile_timer) {
  CreateConstructor(info, JsPercentileTimer::constructor, "percentileTimer");
}

NAN_METHOD(percentile_dist_summary) {
  CreateConstructor(info, JsPercentileDistSummary::constructor,
                    "percentileDistSummary");
}

NAN_METHOD(timer) { CreateConstructor(info, JsTimer::constructor, "timer"); }

NAN_METHOD(long_task_timer) {
  CreateConstructor(info, JsLongTaskTimer::constructor, "longTaskTimer");
}

NAN_METHOD(gauge) { CreateConstructor(info, JsGauge::constructor, "gauge"); }

Nan::Persistent<Function> JsCounter::constructor;
Nan::Persistent<Function> JsDCounter::constructor;
Nan::Persistent<Function> JsIntervalCounter::constructor;
Nan::Persistent<Function> JsTimer::constructor;
Nan::Persistent<Function> JsLongTaskTimer::constructor;
Nan::Persistent<Function> JsGauge::constructor;
Nan::Persistent<Function> JsMaxGauge::constructor;
Nan::Persistent<Function> JsDistSummary::constructor;
Nan::Persistent<Function> JsBucketCounter::constructor;
Nan::Persistent<Function> JsBucketDistSummary::constructor;
Nan::Persistent<Function> JsBucketTimer::constructor;
Nan::Persistent<Function> JsPercentileTimer::constructor;
Nan::Persistent<Function> JsPercentileDistSummary::constructor;

NAN_MODULE_INIT(JsCounter::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsCounter").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "add", Add);
  Nan::SetPrototypeMethod(tpl, "increment", Increment);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsCounter").ToLocalChecked(), tpl->GetFunction());
}

NAN_METHOD(JsCounter::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsCounter(...)`
    JsCounter* obj = new JsCounter(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsCounter::Increment) {
  long value = (long)(info[0]->IsUndefined() ? 1 : info[0]->NumberValue());
  JsCounter* ctr = Nan::ObjectWrap::Unwrap<JsCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsCounter::Add) {
  long value = (long)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());

  JsCounter* ctr = Nan::ObjectWrap::Unwrap<JsCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsCounter::Count) {
  JsCounter* ctr = Nan::ObjectWrap::Unwrap<JsCounter>(info.This());
  double count = ctr->counter_->Count();
  info.GetReturnValue().Set(count);
}

JsCounter::JsCounter(IdPtr id) : counter_{atlas_registry.counter(id)} {}

NAN_MODULE_INIT(JsDCounter::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsDCounter").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "add", Add);
  Nan::SetPrototypeMethod(tpl, "increment", Increment);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsDCounter").ToLocalChecked(), tpl->GetFunction());
}

NAN_METHOD(JsDCounter::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsDCounter(...)`
    JsDCounter* obj = new JsDCounter(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsDCounter::Increment) {
  double value = info[0]->IsUndefined() ? 1.0 : info[0]->NumberValue();
  JsDCounter* ctr = Nan::ObjectWrap::Unwrap<JsDCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsDCounter::Add) {
  double value = info[0]->IsUndefined() ? 0.0 : info[0]->NumberValue();

  JsDCounter* ctr = Nan::ObjectWrap::Unwrap<JsDCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsDCounter::Count) {
  JsDCounter* ctr = Nan::ObjectWrap::Unwrap<JsDCounter>(info.This());
  double count = ctr->counter_->Count();
  info.GetReturnValue().Set(count);
}

JsDCounter::JsDCounter(IdPtr id) : counter_{atlas_registry.dcounter(id)} {}

NAN_MODULE_INIT(JsIntervalCounter::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsIntervalCounter").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "secondsSinceLastUpdate",
                          SecondsSinceLastUpdate);
  Nan::SetPrototypeMethod(tpl, "add", Add);
  Nan::SetPrototypeMethod(tpl, "increment", Increment);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsIntervalCounter").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsIntervalCounter::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsIntervalCounter(...)`
    JsIntervalCounter* obj =
        new JsIntervalCounter(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsIntervalCounter::Increment) {
  long value = (long)(info[0]->IsUndefined() ? 1 : info[0]->NumberValue());
  JsIntervalCounter* ctr =
      Nan::ObjectWrap::Unwrap<JsIntervalCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsIntervalCounter::Add) {
  long value = (long)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());

  JsIntervalCounter* ctr =
      Nan::ObjectWrap::Unwrap<JsIntervalCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsIntervalCounter::Count) {
  JsIntervalCounter* ctr =
      Nan::ObjectWrap::Unwrap<JsIntervalCounter>(info.This());
  double count = ctr->counter_->Count();
  info.GetReturnValue().Set(count);
}

NAN_METHOD(JsIntervalCounter::SecondsSinceLastUpdate) {
  JsIntervalCounter* ctr =
      Nan::ObjectWrap::Unwrap<JsIntervalCounter>(info.This());
  double seconds = ctr->counter_->SecondsSinceLastUpdate();
  info.GetReturnValue().Set(seconds);
}

JsIntervalCounter::JsIntervalCounter(IdPtr id)
    : counter_{std::make_shared<atlas::meter::IntervalCounter>(&atlas_registry,
                                                               id)} {}

NAN_MODULE_INIT(JsTimer::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsTimer").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "record", Record);
  Nan::SetPrototypeMethod(tpl, "timeThis", TimeThis);
  Nan::SetPrototypeMethod(tpl, "totalTime", TotalTime);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsTimer").ToLocalChecked(), tpl->GetFunction());
}

NAN_METHOD(JsTimer::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsTimer(...)`
    JsTimer* obj = new JsTimer(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

constexpr long NANOS = 1000L * 1000L * 1000L;
NAN_METHOD(JsTimer::Record) {
  JsTimer* timer = Nan::ObjectWrap::Unwrap<JsTimer>(info.This());

  int64_t seconds = 0, nanos = 0;
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto hrtime = info[0].As<v8::Array>();
    if (hrtime->Length() == 2) {
      seconds = hrtime->Get(0)->NumberValue();
      nanos = hrtime->Get(1)->NumberValue();
    } else {
      Nan::ThrowError(
          "Expecting an array of two elements: seconds, nanos. See "
          "process.hrtime()");
    }
  } else {
    seconds = (long)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
    nanos = (long)(info[1]->IsUndefined() ? 0 : info[1]->NumberValue());
  }
  timer->timer_->Record(std::chrono::nanoseconds(seconds * NANOS + nanos));
}

NAN_METHOD(JsTimer::TimeThis) {
  JsTimer* timer = Nan::ObjectWrap::Unwrap<JsTimer>(info.This());

  if (info.Length() != 1 || !info[0]->IsFunction()) {
    Nan::ThrowError("Expecting a function as the argument to timeThis.");
  } else {
    auto isolate = info.GetIsolate();
    auto context = isolate->GetCurrentContext();
    auto global = context->Global();
    auto function = v8::Handle<Function>::Cast(info[0]);

    const auto& clock = atlas_registry.clock();
    auto start = clock.MonotonicTime();

    auto result = function->Call(global, 0, nullptr);

    auto elapsed_nanos = clock.MonotonicTime() - start;
    timer->timer_->Record(std::chrono::nanoseconds(elapsed_nanos));
    info.GetReturnValue().Set(result);
  }
}

NAN_METHOD(JsTimer::TotalTime) {
  JsTimer* tmr = Nan::ObjectWrap::Unwrap<JsTimer>(info.This());
  double totalTime = tmr->timer_->TotalTime() / 1e9;
  info.GetReturnValue().Set(totalTime);
}

NAN_METHOD(JsTimer::Count) {
  JsTimer* tmr = Nan::ObjectWrap::Unwrap<JsTimer>(info.This());
  double count = tmr->timer_->Count();
  info.GetReturnValue().Set(count);
}

JsTimer::JsTimer(IdPtr id) : timer_{atlas_registry.timer(id)} {}

JsLongTaskTimer::JsLongTaskTimer(IdPtr id)
    : timer_{atlas_registry.long_task_timer(id)} {}

NAN_MODULE_INIT(JsLongTaskTimer::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsLongTaskTimer").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "start", Start);
  Nan::SetPrototypeMethod(tpl, "stop", Stop);
  Nan::SetPrototypeMethod(tpl, "duration", Duration);
  Nan::SetPrototypeMethod(tpl, "activeTasks", ActiveTasks);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsLongTaskTimer").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsLongTaskTimer::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsTimer(...)`
    JsLongTaskTimer* obj =
        new JsLongTaskTimer(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsLongTaskTimer::Start) {
  auto wrapper = Nan::ObjectWrap::Unwrap<JsLongTaskTimer>(info.This());
  auto start = (double)wrapper->timer_->Start();
  info.GetReturnValue().Set(start);
}

NAN_METHOD(JsLongTaskTimer::Stop) {
  auto wrapper = Nan::ObjectWrap::Unwrap<JsLongTaskTimer>(info.This());
  auto id = (int64_t)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
  auto duration = (double)wrapper->timer_->Stop(id);
  info.GetReturnValue().Set(duration);
}

NAN_METHOD(JsLongTaskTimer::Duration) {
  auto wrapper = Nan::ObjectWrap::Unwrap<JsLongTaskTimer>(info.This());
  auto& tmr = wrapper->timer_;
  double duration;
  if (info.Length() > 0) {
    auto id = (int64_t)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
    duration = (double)tmr->Duration(id);
  } else {
    duration = (double)tmr->Duration();
  }
  info.GetReturnValue().Set(duration);
}

NAN_METHOD(JsLongTaskTimer::ActiveTasks) {
  auto wrapper = Nan::ObjectWrap::Unwrap<JsLongTaskTimer>(info.This());
  auto tasks = wrapper->timer_->ActiveTasks();
  info.GetReturnValue().Set(tasks);
}

NAN_MODULE_INIT(JsGauge::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsGauge").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "value", Value);
  Nan::SetPrototypeMethod(tpl, "update", Update);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsGauge").ToLocalChecked(), tpl->GetFunction());
}

NAN_METHOD(JsGauge::New) {
  using namespace v8;

  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsGauge(...)`
    JsGauge* obj = new JsGauge(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsGauge::Update) {
  JsGauge* g = Nan::ObjectWrap::Unwrap<JsGauge>(info.This());
  auto value = info[0]->IsUndefined() ? 0 : info[0]->NumberValue();
  g->gauge_->Update(value);
}

NAN_METHOD(JsGauge::Value) {
  JsGauge* g = Nan::ObjectWrap::Unwrap<JsGauge>(info.This());
  auto value = g->gauge_->Value();
  info.GetReturnValue().Set(value);
}

JsGauge::JsGauge(IdPtr id) : gauge_{atlas_registry.gauge(id)} {}

NAN_MODULE_INIT(JsMaxGauge::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsMaxGauge").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "value", Value);
  Nan::SetPrototypeMethod(tpl, "update", Update);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsMaxGauge").ToLocalChecked(), tpl->GetFunction());
}

NAN_METHOD(JsMaxGauge::New) {
  const auto argc = info.Length();
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsMaxGauge(...)`
    JsMaxGauge* obj = new JsMaxGauge(idFromValue(info, argc));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsMaxGauge::Value) {
  JsMaxGauge* g = Nan::ObjectWrap::Unwrap<JsMaxGauge>(info.This());
  auto value = g->max_gauge_->Value();
  info.GetReturnValue().Set(value);
}

NAN_METHOD(JsMaxGauge::Update) {
  JsMaxGauge* g = Nan::ObjectWrap::Unwrap<JsMaxGauge>(info.This());
  auto value = info[0]->IsUndefined() ? 0.0 : info[0]->NumberValue();
  g->max_gauge_->Update(value);
}

JsMaxGauge::JsMaxGauge(IdPtr id) : max_gauge_{atlas_registry.max_gauge(id)} {}

NAN_MODULE_INIT(JsDistSummary::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsDistSummary").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "totalAmount", TotalAmount);
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "record", Record);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsDistSummary").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsDistSummary::New) {
  const auto argc = info.Length();
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsDistSummary(...)`
    JsDistSummary* obj = new JsDistSummary(idFromValue(info, argc));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsDistSummary::Count) {
  JsDistSummary* g = Nan::ObjectWrap::Unwrap<JsDistSummary>(info.This());
  auto value = (double)(g->dist_summary_->Count());
  info.GetReturnValue().Set(value);
}

NAN_METHOD(JsDistSummary::TotalAmount) {
  JsDistSummary* g = Nan::ObjectWrap::Unwrap<JsDistSummary>(info.This());
  auto value = (double)(g->dist_summary_->TotalAmount());
  info.GetReturnValue().Set(value);
}

NAN_METHOD(JsDistSummary::Record) {
  JsDistSummary* g = Nan::ObjectWrap::Unwrap<JsDistSummary>(info.This());
  auto value =
      static_cast<int64_t>(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
  g->dist_summary_->Record(value);
}

JsDistSummary::JsDistSummary(IdPtr id)
    : dist_summary_{atlas_registry.distribution_summary(id)} {}

static std::string kEmptyString;
static int64_t GetNumKey(Local<v8::Context> context, Local<Object> object,
                         const char* key) {
  const auto& lookup_key = Nan::New(key).ToLocalChecked();
  auto maybeResult = object->Get(context, lookup_key);
  if (maybeResult.IsEmpty()) {
    return -1;
  }

  auto val = maybeResult.ToLocalChecked()->NumberValue();
  return static_cast<int64_t>(val);
}

static std::string GetStrKey(Local<v8::Context> context, Local<Object> object,
                             const char* key) {
  const auto& lookup_key = Nan::New(key).ToLocalChecked();
  auto maybeResult = object->Get(context, lookup_key);
  if (maybeResult.IsEmpty()) {
    return kEmptyString;
  }

  auto val = maybeResult.ToLocalChecked();
  v8::String::Utf8Value result{val};
  return std::string{*result};
}

static std::chrono::nanoseconds GetDuration(int64_t value,
                                            const std::string& unit) {
  if (unit == "ns") {
    return std::chrono::nanoseconds(value);
  }
  if (unit == "us") {
    return std::chrono::microseconds(value);
  }
  if (unit == "ms") {
    return std::chrono::milliseconds(value);
  }
  if (unit == "s") {
    return std::chrono::seconds(value);
  }
  if (unit == "min") {
    return std::chrono::minutes(value);
  }
  if (unit == "h") {
    return std::chrono::hours(value);
  }

  return std::chrono::nanoseconds::zero();
}

// { "function": "age", "value": 1, "unit": "s" }
// { "function": "ageBiasOld", "value": 1, "unit": "min" }
// { "function": "latency", "value": 500, "unit": "ms" }
// { "function": "latencyBiasOld", "value": 500, "unit": "us" }
// { "function": "bytes", "value": 1024 * 1024}
// { "function": "decimal", "value": 20000 }
//
static Maybe<BucketFunction> bucketFuncFromObject(v8::Isolate* isolate,
                                                  Local<Object> object) {
  const char* kUsageError =
      "Bucket Function Generator expects an object with a key 'function'"
      ", a key 'value', and an optional key 'unit' (for generators that take a "
      "time unit)";

  using atlas::meter::bucket_functions::Age;
  using atlas::meter::bucket_functions::AgeBiasOld;
  using atlas::meter::bucket_functions::Latency;
  using atlas::meter::bucket_functions::LatencyBiasSlow;
  using atlas::meter::bucket_functions::Bytes;
  using atlas::meter::bucket_functions::Decimal;

  auto context = isolate->GetCurrentContext();
  auto props = object->GetOwnPropertyNames(context).ToLocalChecked();
  if (props->Length() < 2 || props->Length() > 3) {
    Nan::ThrowError(kUsageError);
  }

  const auto& function = GetStrKey(context, object, "function");
  if (function.empty()) {
    Nan::ThrowError(kUsageError);
    return v8::Nothing<BucketFunction>();
  }

  const auto value = GetNumKey(context, object, "value");
  if (value < 0) {
    Nan::ThrowError(kUsageError);
    return v8::Nothing<BucketFunction>();
  }

  if (function == "bytes") {
    return v8::Just(Bytes(value));
  }
  if (function == "decimal") {
    return v8::Just(Decimal(value));
  }
  // all others require a duration
  const auto& unit = GetStrKey(context, object, "unit");
  const auto& duration = GetDuration(value, unit);
  if (duration == std::chrono::nanoseconds::zero()) {
    std::ostringstream os;
    os << "Invalid duration specified: unit=" << unit << " value=" << value
       << ", valid units are: ns, us, ms, s, min, h";
    Nan::ThrowError(os.str().c_str());
  }

  if (function == "age") {
    return v8::Just(Age(duration));
  }
  if (function == "ageBiasOld") {
    return v8::Just(AgeBiasOld(duration));
  }
  if (function == "latency") {
    return v8::Just(Latency(duration));
  }
  if (function == "latencyBiasSlow") {
    return v8::Just(LatencyBiasSlow(duration));
  }

  std::ostringstream os;
  os << "Unknown bucket generator function: " << function
     << ": valid options are: age, ageBiasOld, latency, latencyBiasSlow, "
        "bytes, decimal";
  Nan::ThrowError(os.str().c_str());
  return v8::Nothing<BucketFunction>();
}

NAN_MODULE_INIT(JsBucketCounter::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsBucketCounter").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "record", Record);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsBucketCounter").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsBucketCounter::New) {
  const auto argc = info.Length();
  if (argc < 2) {
    Nan::ThrowError(
        "Need at least two arguments: a name, and an object describing bucket "
        "generating function");
    return;
  }

  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsBucketCounter(...)`
    const auto& bucket_function =
        bucketFuncFromObject(info.GetIsolate(), info[argc - 1]->ToObject());
    auto obj = new JsBucketCounter(idFromValue(info, argc - 1),
                                   bucket_function.FromJust());
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsBucketCounter::Record) {
  auto g = Nan::ObjectWrap::Unwrap<JsBucketCounter>(info.This());
  auto value = (uint64_t)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
  g->bucket_counter_->Record(value);
}

JsBucketCounter::JsBucketCounter(IdPtr id, BucketFunction bucket_function)
    : bucket_counter_{std::make_shared<atlas::meter::BucketCounter>(
          &atlas_registry, id, bucket_function)} {}

NAN_MODULE_INIT(JsBucketDistSummary::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsBucketDistSummary").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "record", Record);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsBucketDistSummary").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsBucketDistSummary::New) {
  const auto argc = info.Length();
  if (argc < 2) {
    Nan::ThrowError(
        "Need at least two arguments: a name, and an object describing bucket "
        "generating function");
    return;
  }

  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsBucketDistSummary(...)`
    const auto& bucket_function =
        bucketFuncFromObject(info.GetIsolate(), info[argc - 1]->ToObject());
    auto obj = new JsBucketDistSummary(idFromValue(info, argc - 1),
                                       bucket_function.FromJust());
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsBucketDistSummary::Record) {
  auto g = Nan::ObjectWrap::Unwrap<JsBucketDistSummary>(info.This());
  auto value = (uint64_t)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
  g->bucket_dist_summary_->Record(value);
}

JsBucketDistSummary::JsBucketDistSummary(IdPtr id,
                                         BucketFunction bucket_function)
    : bucket_dist_summary_{
          std::make_shared<atlas::meter::BucketDistributionSummary>(
              &atlas_registry, id, bucket_function)} {}

NAN_MODULE_INIT(JsBucketTimer::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsBucketTimer").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "record", Record);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsBucketTimer").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsBucketTimer::New) {
  const auto argc = info.Length();
  if (argc < 2) {
    Nan::ThrowError(
        "Need at least two arguments: a name, and an object describing bucket "
        "generating function");
    return;
  }

  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsBucketTimer(...)`
    const auto& bucket_function =
        bucketFuncFromObject(info.GetIsolate(), info[argc - 1]->ToObject());
    auto obj = new JsBucketTimer(idFromValue(info, argc - 1),
                                 bucket_function.FromJust());
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsBucketTimer::Record) {
  auto g = Nan::ObjectWrap::Unwrap<JsBucketTimer>(info.This());
  int64_t seconds = 0, nanos = 0;
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto hrtime = info[0].As<v8::Array>();
    if (hrtime->Length() == 2) {
      seconds = hrtime->Get(0)->NumberValue();
      nanos = hrtime->Get(1)->NumberValue();
    } else {
      Nan::ThrowError(
          "Expecting an array of two elements: seconds, nanos. See "
          "process.hrtime()");
    }
  } else {
    seconds = (long)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
    nanos = (long)(info[1]->IsUndefined() ? 0 : info[1]->NumberValue());
  }
  g->bucket_timer_->Record(std::chrono::nanoseconds(seconds * NANOS + nanos));
}

JsBucketTimer::JsBucketTimer(IdPtr id, BucketFunction bucket_function)
    : bucket_timer_{std::make_shared<atlas::meter::BucketTimer>(
          &atlas_registry, id, bucket_function)} {}

NAN_MODULE_INIT(JsPercentileTimer::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsPercentileTimer").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "record", Record);
  Nan::SetPrototypeMethod(tpl, "percentile", Percentile);
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "totalTime", TotalTime);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsPercentileTimer").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsPercentileTimer::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsPercentileTimer(...)`
    auto obj = new JsPercentileTimer(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsPercentileTimer::Record) {
  auto t = Nan::ObjectWrap::Unwrap<JsPercentileTimer>(info.This());
  int64_t seconds = 0, nanos = 0;
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto hrtime = info[0].As<v8::Array>();
    if (hrtime->Length() == 2) {
      seconds = hrtime->Get(0)->NumberValue();
      nanos = hrtime->Get(1)->NumberValue();
    } else {
      Nan::ThrowError(
          "Expecting an array of two elements: seconds, nanos. See "
          "process.hrtime()");
    }
  } else if (info.Length() == 2) {
    seconds = (long)(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
    nanos = (long)(info[1]->IsUndefined() ? 0 : info[1]->NumberValue());
  } else {
    Nan::ThrowError("Expecting two numbers: seconds and nanoseconds");
  }

  t->perc_timer_->Record(std::chrono::nanoseconds(seconds * NANOS + nanos));
}

NAN_METHOD(JsPercentileTimer::TotalTime) {
  auto tmr = Nan::ObjectWrap::Unwrap<JsPercentileTimer>(info.This());
  double totalTime = tmr->perc_timer_->TotalTime() / 1e9;
  info.GetReturnValue().Set(totalTime);
}

NAN_METHOD(JsPercentileTimer::Count) {
  auto tmr = Nan::ObjectWrap::Unwrap<JsPercentileTimer>(info.This());
  double count = tmr->perc_timer_->Count();
  info.GetReturnValue().Set(count);
}

NAN_METHOD(JsPercentileTimer::Percentile) {
  auto t = Nan::ObjectWrap::Unwrap<JsPercentileTimer>(info.This());
  if (info.Length() == 0) {
    Nan::ThrowError(
        "Need the percentile to compute as a number from 0.0 to 100.0");
    return;
  }
  auto n = info[0]->NumberValue();
  auto value = t->perc_timer_->Percentile(n);
  info.GetReturnValue().Set(value);
}

JsPercentileTimer::JsPercentileTimer(IdPtr id)
    : perc_timer_{std::make_shared<atlas::meter::PercentileTimer>(
          &atlas_registry, id)} {}

NAN_MODULE_INIT(JsPercentileDistSummary::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsPercentileDistSummary").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "record", Record);
  Nan::SetPrototypeMethod(tpl, "percentile", Percentile);
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "totalAmount", TotalAmount);

  constructor.Reset(tpl->GetFunction());
  Nan::Set(target, Nan::New("JsPercentileDistSummary").ToLocalChecked(),
           tpl->GetFunction());
}

NAN_METHOD(JsPercentileDistSummary::New) {
  if (info.IsConstructCall()) {
    // Invoked as constructor: `new JsPercentileDistSummary(...)`
    auto obj = new JsPercentileDistSummary(idFromValue(info, info.Length()));
    obj->Wrap(info.This());
    info.GetReturnValue().Set(info.This());
  } else {
    Nan::ThrowError("not implemented");
  }
}

NAN_METHOD(JsPercentileDistSummary::Record) {
  auto t = Nan::ObjectWrap::Unwrap<JsPercentileDistSummary>(info.This());
  auto value =
      static_cast<int64_t>(info[0]->IsUndefined() ? 0 : info[0]->NumberValue());
  t->perc_dist_summary_->Record(value);
}

NAN_METHOD(JsPercentileDistSummary::Count) {
  auto d = Nan::ObjectWrap::Unwrap<JsPercentileDistSummary>(info.This());
  auto value = static_cast<double>(d->perc_dist_summary_->Count());
  info.GetReturnValue().Set(value);
}

NAN_METHOD(JsPercentileDistSummary::TotalAmount) {
  auto d = Nan::ObjectWrap::Unwrap<JsPercentileDistSummary>(info.This());
  auto value = static_cast<double>(d->perc_dist_summary_->TotalAmount());
  info.GetReturnValue().Set(value);
}

NAN_METHOD(JsPercentileDistSummary::Percentile) {
  auto d = Nan::ObjectWrap::Unwrap<JsPercentileDistSummary>(info.This());
  if (info.Length() == 0) {
    Nan::ThrowError(
        "Need the percentile to compute as a number from 0.0 to 100.0");
    return;
  }
  auto n = info[0]->NumberValue();
  auto value = d->perc_dist_summary_->Percentile(n);
  info.GetReturnValue().Set(value);
}

JsPercentileDistSummary::JsPercentileDistSummary(IdPtr id)
    : perc_dist_summary_{
          std::make_shared<atlas::meter::PercentileDistributionSummary>(
              &atlas_registry, id)} {}
