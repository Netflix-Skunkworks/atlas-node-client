#include "utils.h"
#include <atlas/atlas_client.h>
#include <iostream>
#include <sstream>
#include <unordered_set>

using atlas::meter::IdPtr;
using atlas::meter::Tags;
using atlas::util::StartsWith;
using atlas::util::StrRef;
using atlas::util::intern_str;

bool dev_mode = false;

static bool empty_or_null(StrRef r) { return r.is_null() || r.length() == 0; }

static bool is_key_restricted(StrRef k) {
  return StartsWith(k, "nf.") || StartsWith(k, "atlas.");
}

static std::unordered_set<StrRef> kValidNfTags = {
    intern_str("nf.node"),          intern_str("nf.cluster"),
    intern_str("nf.app"),           intern_str("nf.asg"),
    intern_str("nf.stack"),         intern_str("nf.ami"),
    intern_str("nf.vmtype"),        intern_str("nf.zone"),
    intern_str("nf.region"),        intern_str("nf.account"),
    intern_str("nf.country"),       intern_str("nf.task"),
    intern_str("nf.country.rollup")};

static auto kDsType = intern_str("atlas.dstype");
static auto kLegacy = intern_str("atlas.legacy");
static auto kNameRef = intern_str("name");

static bool is_user_key_invalid(StrRef k) noexcept {
  if (StartsWith(k, "atlas.")) {
    return k != kDsType && k != kLegacy;
  }

  if (StartsWith(k, "nf.")) {
    return kValidNfTags.find(k) == kValidNfTags.end();
  }

  return false;
}

static constexpr size_t MAX_KEY_LENGTH = 60;
static constexpr size_t MAX_VAL_LENGTH = 120;
static constexpr size_t MAX_USER_TAGS = 20;
static constexpr size_t MAX_NAME_LENGTH = 255;
static void throw_if_invalid(const std::string& name, const Tags& tags) {
  std::ostringstream name_err;
  auto invalid_name = false;
  if (name.empty()) {
    name_err << "Name is required, cannot be empty\n";
    invalid_name = true;
  } else if (name.length() > MAX_NAME_LENGTH) {
    name_err << "Name '" << name
             << "' exceeds length limits. Length=" << name.length() << " > "
             << MAX_NAME_LENGTH << "\n";
    invalid_name = true;
  }

  auto invalid_tags = false;
  std::ostringstream tag_errors;
  size_t user_tags = 0;
  for (const auto& kv : tags) {
    const auto& k_ref = kv.first;
    const auto& v_ref = kv.second;

    if (empty_or_null(k_ref) || empty_or_null(v_ref)) {
      invalid_tags = true;
      tag_errors << "A tag cannot have an empty key or value\n";
      continue;
    }

    auto k_length = k_ref.length();
    auto v_length = v_ref.length();
    if (k_length > MAX_KEY_LENGTH || v_length > MAX_VAL_LENGTH) {
      invalid_tags = true;
      tag_errors << "Tag " << k_ref.get() << "=" << v_ref.get()
                 << " exceeds length limits. ";
      if (k_length > MAX_KEY_LENGTH) {
        tag_errors << "Key length=" << k_length << " > " << MAX_KEY_LENGTH;
      }
      if (v_length > MAX_VAL_LENGTH) {
        if (k_length > MAX_KEY_LENGTH) {
          tag_errors << ", ";
        }
        tag_errors << "Value length=" << v_length << " > " << MAX_VAL_LENGTH;
      }
      tag_errors << '\n';
    }

    if (!is_key_restricted(k_ref)) {
      ++user_tags;
    }

    if (is_user_key_invalid(k_ref)) {
      invalid_tags = true;
      tag_errors << "Tag key " << k_ref.get()
                 << " is using a reserved namespace\n";
    }
  }

  if (user_tags > MAX_USER_TAGS) {
    invalid_tags = true;
    tag_errors << "Too many user tags. There is a " << MAX_USER_TAGS
               << " user tags limit. Detected user tags = " << user_tags
               << '\n';
  }

  if (invalid_name || invalid_tags) {
    std::string err_msg;
    if (invalid_name) {
      err_msg = name_err.str();
    } else {
      err_msg = "Metric with name='" + name + "' contains invalid tags\n";
    }

    if (invalid_tags) {
      err_msg += tag_errors.str();
    }

    // remove the trailing newline
    err_msg[err_msg.length() - 1] = '\0';
    Nan::ThrowError(err_msg.c_str());
  }
}

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
      tags->add(k.c_str(), v.c_str());
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
  if (name.empty()) {
    Nan::ThrowError("Cannot create a metric with an empty name");
  }

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

  if (dev_mode) {
    throw_if_invalid(name, tags);
  }
  return atlas_registry.CreateId(name, tags);
}
