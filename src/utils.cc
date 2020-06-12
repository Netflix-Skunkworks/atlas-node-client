#include "utils.h"
#include "atlas.h"
#include <atlas/meter/validation.h>
#include <iostream>
#include <sstream>
#include <unordered_set>

using atlas::meter::AnalyzeTags;
using atlas::meter::IdPtr;
using atlas::meter::Tags;
using atlas::meter::ValidationIssue;
using atlas::util::intern_str;
using atlas::util::StartsWith;
using atlas::util::StrRef;

bool dev_mode = false;

std::ostream& operator<<(std::ostream& os, const Tags& tags) {
  os << '{';
  auto first = true;
  for (const auto& kv : tags) {
    if (first) {
      first = false;
    } else {
      os << ", ";
    }
    os << kv.first.get() << '=' << kv.second.get();
  }
  os << '}';
  return os;
}

static void throw_if_invalid(const std::string& name, const Tags& tags) {
  Tags to_check{tags};
  to_check.add("name", name.c_str());

  auto res = atlas::meter::AnalyzeTags(to_check);
  // no warnings or errors found
  if (res.empty()) {
    return;
  }

  auto errs = 0;
  std::ostringstream err_msg;
  err_msg << "[name:'" << name << "', tags:" << tags << "]:\n";
  for (const auto& issue : res) {
    if (issue.level == ValidationIssue::Level::ERROR) {
      ++errs;
    }

    err_msg << '\t' << issue.ToString() << '\n';
  }

  // do not throw if warnings only
  if (errs > 0) {
    auto err = err_msg.str();
    Nan::ThrowError(err.c_str());
  }
}

bool tagsFromObject(v8::Isolate* isolate, const v8::Local<v8::Object>& object,
                    Tags* tags, std::string* error_msg) {
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
      if (k.empty()) {
        *error_msg = "Cannot have an empty key when specifying tags";
        return false;
      }
      if (v.empty()) {
        *error_msg = "Cannot have an empty value for key '" + k +
                     "' when specifying tags";
        return false;
      }
      tags->add(k.c_str(), v.c_str());
    }
  }
  return true;
}

IdPtr idFromValue(const Nan::FunctionCallbackInfo<v8::Value>& info, int argc) {
  std::string err_msg;
  Tags tags;
  std::string name;
  auto r = atlas_registry();

  if (argc == 0) {
    err_msg = "Need at least one argument to specify a metric name";
    goto error;
  } else if (argc > 2) {
    err_msg =
        "Expecting at most two arguments: a name and an object describing tags";
    goto error;
  }
  name = *Nan::Utf8String(info[0]);
  if (name.empty()) {
    err_msg = "Cannot create a metric with an empty name";
    goto error;
  }

  if (argc == 2) {
    // read the object which should just have string keys and string values
    const auto& maybe_o = info[1];
    if (maybe_o->IsObject()) {
      auto context = Nan::GetCurrentContext();
      if (!tagsFromObject(info.GetIsolate(),
                          maybe_o->ToObject(context).ToLocalChecked(), &tags,
                          &err_msg)) {
        err_msg += " for metric name '" + name + "'";
        goto error;
      }
    } else {
      err_msg =
          "Expected an object with string keys and string values as the second "
          "argument";
      goto error;
    }
  }

  // do expensive validation only in dev mode
  if (dev_mode) {
    throw_if_invalid(name, tags);
  }
  return r->CreateId(name, tags);
error:
  if (dev_mode) {
    Nan::ThrowError(err_msg.c_str());
  } else {
    fprintf(stderr, "Error creating atlas metric ID: %s\n", err_msg.c_str());
  }
  tags.add("atlas.invalid", "true");
  return r->CreateId("invalid", tags);
}
