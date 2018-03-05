#include "start_stop.h"
#include "functions.h"

using v8::FunctionTemplate;
using Nan::New;
using Nan::Set;
using Nan::GetFunction;

// atlas.cc represents the top level of the module.
// C++ constructs that are exposed to javascript are exported here

NAN_MODULE_INIT(InitAll) {
  Set(target, New("start").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(start)).ToLocalChecked());

  Set(target, New("stop").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(stop)).ToLocalChecked());

  Set(target, New("counter").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(counter)).ToLocalChecked());

  Set(target, New("dcounter").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(dcounter)).ToLocalChecked());

  Set(target, New("intervalCounter").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(interval_counter)).ToLocalChecked());

  Set(target, New("timer").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(timer)).ToLocalChecked());

  Set(target, New("longTaskTimer").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(long_task_timer)).ToLocalChecked());

  Set(target, New("gauge").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(gauge)).ToLocalChecked());

  Set(target, New("maxGauge").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(max_gauge)).ToLocalChecked());

  Set(target, New("distSummary").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(dist_summary)).ToLocalChecked());

  Set(target, New("bucketCounter").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(bucket_counter)).ToLocalChecked());

  Set(target, New("bucketDistSummary").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(bucket_dist_summary)).ToLocalChecked());

  Set(target, New("bucketTimer").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(bucket_timer)).ToLocalChecked());

  Set(target, New("percentileTimer").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(percentile_timer)).ToLocalChecked());

  Set(target, New("percentileDistSummary").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(percentile_dist_summary))
          .ToLocalChecked());

  Set(target, New("measurements").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(measurements)).ToLocalChecked());

  Set(target, New("config").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(config)).ToLocalChecked());

  Set(target, New("push").ToLocalChecked(),
      GetFunction(New<FunctionTemplate>(push)).ToLocalChecked());

  JsCounter::Init(target);
  JsDCounter::Init(target);
  JsIntervalCounter::Init(target);
  JsTimer::Init(target);
  JsLongTaskTimer::Init(target);
  JsGauge::Init(target);
  JsMaxGauge::Init(target);
  JsDistSummary::Init(target);
  JsBucketCounter::Init(target);
  JsBucketDistSummary::Init(target);
  JsBucketTimer::Init(target);
  JsPercentileTimer::Init(target);
  JsPercentileDistSummary::Init(target);
}

NODE_MODULE(Atlas, InitAll)
