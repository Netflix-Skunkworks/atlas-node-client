#include "utils.h"
#include <atlas/atlas_client.h>

using atlas::meter::IdPtr;
using atlas::meter::Tags;

bool tagsFromObject(v8::Isolate* isolate, const v8::Local<v8::Object>& object,
                    Tags* tags) {
  auto context = isolate->GetCurrentContext();
  auto maybe_props = object->GetOwnPropertyNames(context);
  if (!maybe_props.IsEmpty()) {
    auto props = maybe_props.ToLocalChecked();
    auto n = props->Length();
    for (uint32_t i = 0; i < n; ++i) {
      const auto& key = props->Get(i);
      const auto& value = object->Get(key);
      const auto& k = std::string(*Nan::Utf8String(key));
      const auto& v = std::string(*Nan::Utf8String(value));
      if (k.empty()) {
        Nan::ThrowError("Cannot have an empty key when specifying tags");
      }
      if (v.empty()) {
        auto err_msg = "Cannot have an empty value for key '" + k +
                       "' when specifying tags";
        Nan::ThrowError(err_msg.c_str());
      }
      (*tags)[k] = v;
    }
    return true;
  } else {
    return false;
  }
}

IdPtr idFromValue(const Nan::FunctionCallbackInfo<v8::Value>& info, int argc) {
  if (argc == 0) {
    Nan::ThrowError("Need at least one argument to specify a metric name");
  } else if (argc > 2) {
    Nan::ThrowError(
        "Expecting at most two arguments: a name and an object describing "
        "tags");
  }
  std::string name{*Nan::Utf8String(info[0])};

  Tags tags;
  if (argc == 2) {
    // read the object which should just have string keys and string values
    const auto& maybe_o = info[1];
    if (maybe_o->IsObject()) {
      tagsFromObject(info.GetIsolate(), maybe_o->ToObject(), &tags);
    } else {
      Nan::ThrowError(
          "Expected an object with string keys and string values as the second "
          "argument");
    }
  }
  return atlas_registry.CreateId(name, tags);
}
