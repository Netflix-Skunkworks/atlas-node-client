#include "functions.h"
#include "atlas.h"
#include "utils.h"
#include <atlas/meter/validation.h>
#include <chrono>
#include <sstream>

using atlas::meter::AnalyzeTags;
using atlas::meter::BucketFunction;
using atlas::meter::IdPtr;
using atlas::meter::Measurement;
using atlas::meter::Measurements;
using atlas::meter::Tags;
using atlas::meter::ValidationIssue;
using atlas::meter::ValidationIssues;
using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Maybe;
using v8::Object;

NAN_METHOD(set_dev_mode) {
  if (info.Length() == 1 && info[0]->IsBoolean()) {
    auto b = Nan::To<bool>(info[0]).FromJust();
    dev_mode = b;
  }
}

static void addTags(v8::Isolate* isolate, const v8::Local<v8::Object>& object,
                    Tags* tags) {
  auto context = isolate->GetCurrentContext();
  auto maybe_props = object->GetOwnPropertyNames(context);
  if (!maybe_props.IsEmpty()) {
    auto props = maybe_props.ToLocalChecked();
    auto n = props->Length();
    for (uint32_t i = 0; i < n; ++i) {
      const auto& key = props->Get(context, i).ToLocalChecked();
      const auto& value = object->Get(context, key).ToLocalChecked();
      const auto& k = std::string(*Nan::Utf8String(key));
      const auto& v = std::string(*Nan::Utf8String(value));
      tags->add(k.c_str(), v.c_str());
    }
  }
}

NAN_METHOD(validate_name_tags) {
  const char* err_msg = nullptr;
  std::string name;
  ValidationIssues res;
  Tags tags;
  auto context = Nan::GetCurrentContext();
  auto ret = Nan::New<v8::Array>();

  if (info.Length() > 2) {
    err_msg =
        "Expecting at most two arguments: a name and an object describing tags";
    goto error;
  }

  if (info.Length() >= 1) {
    name = *Nan::Utf8String(info[0]);
    tags.add("name", name.c_str());
  }

  if (info.Length() == 2 && !(info[1]->IsUndefined() || info[1]->IsNull())) {
    const auto& maybe_o = info[1];
    if (maybe_o->IsObject()) {
      addTags(info.GetIsolate(),
              maybe_o->ToObject(Nan::GetCurrentContext()).ToLocalChecked(),
              &tags);
    } else {
      err_msg =
          "Expected an object with string keys and string values as the second "
          "argument";
      goto error;
    }
  }

  res = AnalyzeTags(tags);
  if (res.empty()) {
    return;
  }

  for (const auto& issue : res) {
    auto js_issue = Nan::New<Object>();
    auto level =
        issue.level == ValidationIssue::Level::ERROR ? "ERROR" : "WARN";
    js_issue
        ->Set(context, Nan::New(level).ToLocalChecked(),
              Nan::New(issue.description.c_str()).ToLocalChecked())
        .FromJust();
    ret->Set(context, ret->Length(), js_issue).FromJust();
  }
error:
  if (err_msg != nullptr) {
    auto js_issue = Nan::New<Object>();
    js_issue
        ->Set(context, Nan::New("ERROR").ToLocalChecked(),
              Nan::New(err_msg).ToLocalChecked())
        .FromJust();
    ret->Set(context, ret->Length(), js_issue).FromJust();
  }

  info.GetReturnValue().Set(ret);
}

NAN_METHOD(measurements) {
  auto context = Nan::GetCurrentContext();
  auto config = atlas_client().GetConfig();
  auto common_tags = config->CommonTags();
  const auto& measurements = atlas_registry()->measurements();

  auto ret = Nan::New<v8::Array>();
  auto name = Nan::New("name").ToLocalChecked();

  for (const auto& m : measurements) {
    auto measurement = Nan::New<Object>();
    auto tags = Nan::New<Object>();
    tags->Set(context, name, Nan::New(m.id->Name()).ToLocalChecked()).FromJust();

    const auto& t = m.id->GetTags();
    for (const auto& kv : common_tags) {
      tags->Set(context, Nan::New(kv.first.get()).ToLocalChecked(),
                Nan::New(kv.second.get()).ToLocalChecked())
          .FromJust();
    }
    for (const auto& kv : t) {
      tags->Set(context, Nan::New(kv.first.get()).ToLocalChecked(),
                Nan::New(kv.second.get()).ToLocalChecked())
          .FromJust();
    }
    measurement->Set(context, Nan::New("tags").ToLocalChecked(), tags).FromJust();
    measurement
        ->Set(context, Nan::New("value").ToLocalChecked(), Nan::New(m.value))
        .FromJust();
    ret->Set(context, ret->Length(), measurement).FromJust();
  }

  info.GetReturnValue().Set(ret);
}

NAN_METHOD(config) {
  auto currentCfg = atlas_client().GetConfig();
  const auto& endpoints = currentCfg->EndpointConfiguration();
  const auto& log = currentCfg->LogConfiguration();
  const auto& http = currentCfg->HttpConfiguration();
  auto context = Nan::GetCurrentContext();
  auto ret = Nan::New<Object>();
  ret->Set(context, Nan::New("evaluateUrl").ToLocalChecked(),
           Nan::New(endpoints.evaluate.c_str()).ToLocalChecked())
      .FromJust();
  ret->Set(context, Nan::New("subscriptionsUrl").ToLocalChecked(),
           Nan::New(endpoints.subscriptions.c_str()).ToLocalChecked())
      .FromJust();
  ret->Set(context, Nan::New("publishUrl").ToLocalChecked(),
           Nan::New(endpoints.publish.c_str()).ToLocalChecked())
      .FromJust();
  ret->Set(context, Nan::New("subscriptionsRefreshMillis").ToLocalChecked(),
           Nan::New(static_cast<double>(currentCfg->SubRefreshMillis())))
      .FromJust();
  ret->Set(context, Nan::New("batchSize").ToLocalChecked(),
           Nan::New(http.batch_size))
      .FromJust();
  ret->Set(context, Nan::New("connectTimeout").ToLocalChecked(),
           Nan::New(http.connect_timeout))
      .FromJust();
  ret->Set(context, Nan::New("readTimeout").ToLocalChecked(),
           Nan::New(http.read_timeout))
      .FromJust();
  ret->Set(context, Nan::New("dumpMetrics").ToLocalChecked(),
           Nan::New(log.dump_metrics))
      .FromJust();
  ret->Set(context, Nan::New("dumpSubscriptions").ToLocalChecked(),
           Nan::New(log.dump_subscriptions))
      .FromJust();
  ret->Set(context, Nan::New("publishEnabled").ToLocalChecked(),
           Nan::New(currentCfg->IsMainEnabled()))
      .FromJust();

  auto pubCfg = Nan::New<v8::Array>();
  for (const auto& expr : currentCfg->PublishConfig()) {
    pubCfg
        ->Set(context, pubCfg->Length(),
              Nan::New(expr.c_str()).ToLocalChecked())
        .FromJust();
  }
  ret->Set(context, Nan::New("publishConfig").ToLocalChecked(), pubCfg).FromJust();
  auto commonTags = Nan::New<Object>();
  for (const auto& kv : currentCfg->CommonTags()) {
    commonTags
        ->Set(context, Nan::New(kv.first.get()).ToLocalChecked(),
              Nan::New(kv.second.get()).ToLocalChecked())
        .FromJust();
  }
  ret->Set(context, Nan::New("commonTags").ToLocalChecked(), commonTags)
      .FromJust();
  ret->Set(context, Nan::New("loggingDirectory").ToLocalChecked(),
           Nan::New(currentCfg->LoggingDirectory().c_str()).ToLocalChecked())
      .FromJust();

  info.GetReturnValue().Set(ret);
}

static Measurement getMeasurement(v8::Isolate* isolate, Local<v8::Value> v) {
  std::string err_msg;
  auto context = Nan::GetCurrentContext();
  auto o = v->ToObject(context).ToLocalChecked();
  auto nameVal =
      Nan::Get(o, Nan::New("name").ToLocalChecked()).ToLocalChecked();
  Nan::Utf8String utf8(nameVal);
  std::string name(*utf8);
  auto timestamp =
      Nan::To<int32_t>(
          Nan::Get(o, Nan::New("timestamp").ToLocalChecked()).ToLocalChecked())
          .FromJust();
  auto value =
      Nan::To<double>(
          Nan::Get(o, Nan::New("value").ToLocalChecked()).ToLocalChecked())
          .FromJust();

  auto tagsObj = Nan::Get(o, Nan::New("tags").ToLocalChecked())
                     .ToLocalChecked()
                     ->ToObject(context)
                     .ToLocalChecked();
  Tags tags;
  tagsFromObject(isolate, tagsObj, &tags, &err_msg);

  auto id = atlas_registry()->CreateId(name, tags);
  return Measurement{id, timestamp, value};
}

NAN_METHOD(push) {
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto measurements = info[0].As<v8::Array>();
    auto context = Nan::GetCurrentContext();
    Measurements ms;
    for (uint32_t i = 0; i < measurements->Length(); ++i) {
      ms.push_back(getMeasurement(
          info.GetIsolate(), measurements->Get(context, i).ToLocalChecked()));
    }
    atlas_client().Push(ms);
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
  auto context = Nan::GetCurrentContext();
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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsCounter").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  long value =
      (long)(info[0]->IsUndefined()
                 ? 1
                 : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
  JsCounter* ctr = Nan::ObjectWrap::Unwrap<JsCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsCounter::Add) {
  long value =
      (long)(info[0]->IsUndefined()
                 ? 0
                 : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());

  JsCounter* ctr = Nan::ObjectWrap::Unwrap<JsCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsCounter::Count) {
  JsCounter* ctr = Nan::ObjectWrap::Unwrap<JsCounter>(info.This());
  double count = ctr->counter_->Count();
  info.GetReturnValue().Set(count);
}

JsCounter::JsCounter(IdPtr id) : counter_{atlas_registry()->counter(id)} {}

NAN_MODULE_INIT(JsDCounter::Init) {
  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsDCounter").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "count", Count);
  Nan::SetPrototypeMethod(tpl, "add", Add);
  Nan::SetPrototypeMethod(tpl, "increment", Increment);

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsDCounter").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  double value =
      info[0]->IsUndefined()
          ? 1.0
          : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust();
  JsDCounter* ctr = Nan::ObjectWrap::Unwrap<JsDCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsDCounter::Add) {
  double value =
      info[0]->IsUndefined()
          ? 0.0
          : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust();

  JsDCounter* ctr = Nan::ObjectWrap::Unwrap<JsDCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsDCounter::Count) {
  JsDCounter* ctr = Nan::ObjectWrap::Unwrap<JsDCounter>(info.This());
  double count = ctr->counter_->Count();
  info.GetReturnValue().Set(count);
}

JsDCounter::JsDCounter(IdPtr id) : counter_{atlas_registry()->dcounter(id)} {}

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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsIntervalCounter").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  long value =
      (long)(info[0]->IsUndefined()
                 ? 1
                 : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
  JsIntervalCounter* ctr =
      Nan::ObjectWrap::Unwrap<JsIntervalCounter>(info.This());
  ctr->counter_->Add(value);
}

NAN_METHOD(JsIntervalCounter::Add) {
  long value =
      (long)(info[0]->IsUndefined()
                 ? 0
                 : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());

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
    : counter_{std::make_shared<atlas::meter::IntervalCounter>(atlas_registry(),
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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsTimer").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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

  auto context = Nan::GetCurrentContext();
  int64_t seconds = 0, nanos = 0;
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto hrtime = info[0].As<v8::Array>();
    if (hrtime->Length() == 2) {
      seconds =
          Nan::To<int64_t>(hrtime->Get(context, 0).ToLocalChecked()).FromJust();
      nanos =
          Nan::To<int64_t>(hrtime->Get(context, 1).ToLocalChecked()).FromJust();
    } else {
      Nan::ThrowError(
          "Expecting an array of two elements: seconds, nanos. See "
          "process.hrtime()");
    }
  } else {
    seconds = (long)(info[0]->IsUndefined()
                         ? 0
                         : info[0]->NumberValue(context).FromJust());
    nanos = (long)(info[1]->IsUndefined()
                       ? 0
                       : info[1]->NumberValue(context).FromJust());
  }
  timer->timer_->Record(std::chrono::nanoseconds(seconds * NANOS + nanos));
}

NAN_METHOD(JsTimer::TimeThis) {
  JsTimer* timer = Nan::ObjectWrap::Unwrap<JsTimer>(info.This());

  if (info.Length() != 1 || !info[0]->IsFunction()) {
    Nan::ThrowError("Expecting a function as the argument to timeThis.");
  } else {
    auto context = Nan::GetCurrentContext();
    auto function = Nan::To<v8::Function>(info[0]).ToLocalChecked();

    const auto& clock = atlas_registry()->clock();
    auto start = clock.MonotonicTime();

    auto result =
        Nan::Call(function, context->Global(), 0, nullptr).ToLocalChecked();

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

JsTimer::JsTimer(IdPtr id) : timer_{atlas_registry()->timer(id)} {}

JsLongTaskTimer::JsLongTaskTimer(IdPtr id)
    : timer_{atlas_registry()->long_task_timer(id)} {}

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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsLongTaskTimer").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  auto id = (int64_t)(
      info[0]->IsUndefined()
          ? 0
          : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
  auto duration = (double)wrapper->timer_->Stop(id);
  info.GetReturnValue().Set(duration);
}

NAN_METHOD(JsLongTaskTimer::Duration) {
  auto wrapper = Nan::ObjectWrap::Unwrap<JsLongTaskTimer>(info.This());
  auto& tmr = wrapper->timer_;
  double duration;
  if (info.Length() > 0) {
    auto id = (int64_t)(
        info[0]->IsUndefined()
            ? 0
            : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsGauge").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  auto value = info[0]->IsUndefined()
                   ? 0
                   : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust();
  g->gauge_->Update(value);
}

NAN_METHOD(JsGauge::Value) {
  JsGauge* g = Nan::ObjectWrap::Unwrap<JsGauge>(info.This());
  auto value = g->gauge_->Value();
  info.GetReturnValue().Set(value);
}

JsGauge::JsGauge(IdPtr id) : gauge_{atlas_registry()->gauge(id)} {}

NAN_MODULE_INIT(JsMaxGauge::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsMaxGauge").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "value", Value);
  Nan::SetPrototypeMethod(tpl, "update", Update);

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsMaxGauge").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  auto value = info[0]->IsUndefined()
                   ? 0.0
                   : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust();
  g->max_gauge_->Update(value);
}

JsMaxGauge::JsMaxGauge(IdPtr id)
    : max_gauge_{atlas_registry()->max_gauge(id)} {}

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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsDistSummary").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  auto value = static_cast<int64_t>(
      info[0]->IsUndefined()
          ? 0
          : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
  g->dist_summary_->Record(value);
}

JsDistSummary::JsDistSummary(IdPtr id)
    : dist_summary_{atlas_registry()->distribution_summary(id)} {}

static std::string kEmptyString;
static int64_t GetNumKey(Local<v8::Context> context, Local<Object> object,
                         const char* key) {
  const auto& lookup_key = Nan::New(key).ToLocalChecked();
  auto maybeResult = object->Get(context, lookup_key);
  if (maybeResult.IsEmpty()) {
    return -1;
  }

  auto val = maybeResult.ToLocalChecked()
                 ->NumberValue(Nan::GetCurrentContext())
                 .FromJust();
  return static_cast<int64_t>(val);
}

static std::string GetStrKey(Local<v8::Context> context, Local<Object> object,
                             const char* key) {
  const auto& lookup_key = Nan::New(key).ToLocalChecked();
  auto maybeResult = object->Get(context, lookup_key);
  if (maybeResult.IsEmpty()) {
    return kEmptyString;
  }

  Nan::Utf8String val{maybeResult.ToLocalChecked()};
  return std::string{*val};
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
  using atlas::meter::bucket_functions::Bytes;
  using atlas::meter::bucket_functions::Decimal;
  using atlas::meter::bucket_functions::Latency;
  using atlas::meter::bucket_functions::LatencyBiasSlow;

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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsBucketCounter").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
    auto context = Nan::GetCurrentContext();
    const auto& bucket_function = bucketFuncFromObject(
        info.GetIsolate(), info[argc - 1]->ToObject(context).ToLocalChecked());
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
  auto value = (uint64_t)(
      info[0]->IsUndefined()
          ? 0
          : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
  g->bucket_counter_->Record(value);
}

JsBucketCounter::JsBucketCounter(IdPtr id, BucketFunction bucket_function)
    : bucket_counter_{std::make_shared<atlas::meter::BucketCounter>(
          atlas_registry(), id, bucket_function)} {}

NAN_MODULE_INIT(JsBucketDistSummary::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsBucketDistSummary").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "record", Record);

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsBucketDistSummary").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
    auto context = Nan::GetCurrentContext();
    const auto& bucket_function = bucketFuncFromObject(
        info.GetIsolate(), info[argc - 1]->ToObject(context).ToLocalChecked());
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
  auto value = (uint64_t)(
      info[0]->IsUndefined()
          ? 0
          : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
  g->bucket_dist_summary_->Record(value);
}

JsBucketDistSummary::JsBucketDistSummary(IdPtr id,
                                         BucketFunction bucket_function)
    : bucket_dist_summary_{
          std::make_shared<atlas::meter::BucketDistributionSummary>(
              atlas_registry(), id, bucket_function)} {}

NAN_MODULE_INIT(JsBucketTimer::Init) {
  Nan::HandleScope scope;

  // Prepare constructor template
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("JsBucketTimer").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  // Prototype
  Nan::SetPrototypeMethod(tpl, "record", Record);

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsBucketTimer").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
    auto context = Nan::GetCurrentContext();
    const auto& bucket_function = bucketFuncFromObject(
        info.GetIsolate(), info[argc - 1]->ToObject(context).ToLocalChecked());
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
  auto context = Nan::GetCurrentContext();

  int64_t seconds = 0, nanos = 0;
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto hrtime = info[0].As<v8::Array>();
    if (hrtime->Length() == 2) {
      seconds =
          Nan::To<int64_t>(hrtime->Get(context, 0).ToLocalChecked()).FromJust();
      nanos =
          Nan::To<int64_t>(hrtime->Get(context, 1).ToLocalChecked()).FromJust();
    } else {
      Nan::ThrowError(
          "Expecting an array of two elements: seconds, nanos. See "
          "process.hrtime()");
    }
  } else {
    seconds = (long)(info[0]->IsUndefined()
                         ? 0
                         : info[0]->NumberValue(context).FromJust());
    nanos = (long)(info[1]->IsUndefined()
                       ? 0
                       : info[1]->NumberValue(context).FromJust());
  }
  g->bucket_timer_->Record(std::chrono::nanoseconds(seconds * NANOS + nanos));
}

JsBucketTimer::JsBucketTimer(IdPtr id, BucketFunction bucket_function)
    : bucket_timer_{std::make_shared<atlas::meter::BucketTimer>(
          atlas_registry(), id, bucket_function)} {}

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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsPercentileTimer").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  auto context = Nan::GetCurrentContext();
  int64_t seconds = 0, nanos = 0;
  if (info.Length() == 1 && info[0]->IsArray()) {
    auto hrtime = info[0].As<v8::Array>();
    if (hrtime->Length() == 2) {
      seconds =
          Nan::To<int64_t>(hrtime->Get(context, 0).ToLocalChecked()).FromJust();
      nanos =
          Nan::To<int64_t>(hrtime->Get(context, 1).ToLocalChecked()).FromJust();
    } else {
      Nan::ThrowError(
          "Expecting an array of two elements: seconds, nanos. See "
          "process.hrtime()");
    }
  } else if (info.Length() == 2) {
    seconds = (long)(info[0]->IsUndefined()
                         ? 0
                         : info[0]->NumberValue(context).FromJust());
    nanos = (long)(info[1]->IsUndefined()
                       ? 0
                       : info[1]->NumberValue(context).FromJust());
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
  auto n = info[0]->NumberValue(Nan::GetCurrentContext()).FromJust();
  auto value = t->perc_timer_->Percentile(n);
  info.GetReturnValue().Set(value);
}

JsPercentileTimer::JsPercentileTimer(IdPtr id)
    : perc_timer_{std::make_shared<atlas::meter::PercentileTimer>(
          atlas_registry(), id)} {}

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

  auto context = Nan::GetCurrentContext();
  constructor.Reset(tpl->GetFunction(context).ToLocalChecked());
  Nan::Set(target, Nan::New("JsPercentileDistSummary").ToLocalChecked(),
           tpl->GetFunction(context).ToLocalChecked());
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
  auto value = static_cast<int64_t>(
      info[0]->IsUndefined()
          ? 0
          : info[0]->NumberValue(Nan::GetCurrentContext()).FromJust());
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
  auto n = info[0]->NumberValue(Nan::GetCurrentContext()).FromJust();
  auto value = d->perc_dist_summary_->Percentile(n);
  info.GetReturnValue().Set(value);
}

JsPercentileDistSummary::JsPercentileDistSummary(IdPtr id)
    : perc_dist_summary_{
          std::make_shared<atlas::meter::PercentileDistributionSummary>(
              atlas_registry(), id)} {}
