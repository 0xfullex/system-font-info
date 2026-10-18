#include <napi.h>

#include <cmath>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../orthography/orthography.h"
#include "../sfnt/sfnt.h"
#include "model.h"
#include "platform.h"
#include "request.h"
#include "to_js.h"

namespace sfi {
namespace {

// ---- getFamilies ------------------------------------------------------------

// The request is an internal contract with the JS layer, which has validated the caller's options already (see
// ts/native.ts), so anything malformed here is a defect, reported as a plain Error.
std::vector<std::string> stringArrayMember(Napi::Env env, const Napi::Object& object, const char* key) {
  const Napi::Value value = object.Get(key);
  if (!value.IsArray())
    throw Napi::Error::New(env, std::string("system-font-info: request.") + key + " must be an array");
  const Napi::Array array = value.As<Napi::Array>();
  std::vector<std::string> strings;
  strings.reserve(array.Length());
  for (uint32_t i = 0; i < array.Length(); ++i) {
    const Napi::Value item = array.Get(i);
    if (!item.IsString()) {
      throw Napi::Error::New(env, std::string("system-font-info: request.") + key + " must hold strings only");
    }
    strings.push_back(item.As<Napi::String>().Utf8Value());
  }
  return strings;
}

Request parseRequest(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsObject())
    throw Napi::Error::New(env, "system-font-info: the request must be an object");
  const Napi::Object object = info[0].As<Napi::Object>();
  std::optional<std::vector<std::string>> families;
  if (!object.Get("families").IsUndefined()) families = stringArrayMember(env, object, "families");
  try {
    return makeRequest(stringArrayMember(env, object, "fields"), std::move(families),
                       stringArrayMember(env, object, "orthographies"));
  } catch (const std::exception& e) {
    throw Napi::Error::New(env, e.what());
  }
}

// Enumerates on a worker thread; the result is converted and the promise
// settled back on the JS thread. With `withWork` the promise resolves to
// `{ families, work }`, `work` being what the call did (test hook).
class GetFamiliesWorker : public Napi::AsyncWorker {
 public:
  GetFamiliesWorker(Napi::Env env, Request request, bool withWork)
      : Napi::AsyncWorker(env, "system-font-info:getFamilies"),
        deferred_(Napi::Promise::Deferred::New(env)),
        request_(std::move(request)),
        withWork_(withWork) {}

  Napi::Promise promise() const { return deferred_.Promise(); }

 protected:
  void Execute() override {
    try {
      families_ = platform::enumerateFonts(request_, work_);
    } catch (const std::exception& e) {
      SetError(e.what());
    } catch (...) {
      SetError("system-font-info: unknown error while listing fonts");
    }
  }

  void OnOK() override {
    Napi::Env env = Env();
    try {
      Napi::Array families = familiesToJs(env, families_, request_);
      families_.clear();
      if (!withWork_) {
        deferred_.Resolve(families);
        return;
      }
      Napi::Object work = Napi::Object::New(env);
      work.Set("fontsOpened", Napi::Number::New(env, work_.fontsOpened));
      work.Set("tableEntries", Napi::Number::New(env, work_.tableEntries));
      work.Set("characterSets", Napi::Number::New(env, work_.characterSets));
      Napi::Object result = Napi::Object::New(env);
      result.Set("families", families);
      result.Set("work", work);
      deferred_.Resolve(result);
    } catch (const Napi::Error& e) {
      deferred_.Reject(e.Value());
    }
  }

  void OnError(const Napi::Error& error) override { deferred_.Reject(error.Value()); }

 private:
  Napi::Promise::Deferred deferred_;
  Request request_;
  bool withWork_;
  WorkLog work_;
  std::vector<Family> families_;
};

Napi::Value queueGetFamilies(const Napi::CallbackInfo& info, bool withWork) {
  auto* worker = new GetFamiliesWorker(info.Env(), parseRequest(info), withWork);
  Napi::Promise promise = worker->promise();
  worker->Queue();  // the worker deletes itself after OnOK / OnError
  return promise;
}

Napi::Value getFamilies(const Napi::CallbackInfo& info) { return queueGetFamilies(info, false); }

Napi::Value getFamiliesWithWork(const Napi::CallbackInfo& info) { return queueGetFamilies(info, true); }

// ---- test hooks -------------------------------------------------------------

std::string pathArgument(const Napi::CallbackInfo& info, const char* what) {
  if (info.Length() < 1 || !info[0].IsString()) {
    throw Napi::TypeError::New(info.Env(), std::string(what) + ": path must be a string");
  }
  return info[0].As<Napi::String>().Utf8Value();
}

Napi::Value registerFont(const Napi::CallbackInfo& info) {
  std::string path = pathArgument(info, "registerFont");
  try {
    platform::registerFontForTesting(path);
  } catch (const std::exception& e) {
    throw Napi::Error::New(info.Env(), e.what());
  }
  return info.Env().Undefined();
}

Napi::Value unregisterFont(const Napi::CallbackInfo& info) {
  std::string path = pathArgument(info, "unregisterFont");
  try {
    platform::unregisterFontForTesting(path);
  } catch (const std::exception& e) {
    throw Napi::Error::New(info.Env(), e.what());
  }
  return info.Env().Undefined();
}

// Reads one face of a font file through the same table reader the platform
// backends use, and returns its `opentype` object. The optional third argument
// lists the entries to build (`tables`, `names`, `head`, ...); all by default.
Napi::Value readFontFile(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  std::string path = pathArgument(info, "readFontFile");
  if (info.Length() < 2 || !info[1].IsNumber()) throw Napi::TypeError::New(env, "readFontFile: index must be a number");
  double requested = info[1].As<Napi::Number>().DoubleValue();

  auto file = sfnt::FontFile::open(path);
  if (!file) throw Napi::Error::New(env, "readFontFile: cannot open '" + path + "' as an sfnt font or collection");
  // Compared as doubles before any conversion: casting a double outside the
  // range of size_t (or a NaN) to size_t is undefined behavior.
  if (!(requested >= 0) || requested >= static_cast<double>(file->faceCount()) || requested != std::trunc(requested)) {
    throw Napi::RangeError::New(env, "readFontFile: face index " + info[1].ToString().Utf8Value() +
                                         " is out of range; '" + path + "' has " + std::to_string(file->faceCount()) +
                                         " face(s)");
  }
  size_t index = static_cast<size_t>(requested);

  sfnt::OpenTypeSelection selection = sfnt::OpenTypeSelection::all();
  if (info.Length() > 2 && !info[2].IsUndefined()) {
    if (!info[2].IsArray()) throw Napi::TypeError::New(env, "readFontFile: entries must be an array of entry names");
    selection = {};
    const Napi::Array names = info[2].As<Napi::Array>();
    for (uint32_t i = 0; i < names.Length(); ++i) {
      const Napi::Value name = names.Get(i);
      const auto entry =
          name.IsString() ? sfnt::openTypeEntryFromName(name.As<Napi::String>().Utf8Value()) : std::nullopt;
      if (!entry)
        throw Napi::TypeError::New(env, "readFontFile: unknown opentype entry " + name.ToString().Utf8Value());
      selection.select(*entry);
    }
  }

  try {
    const sfnt::TableFetcher fetch = [&](uint32_t tag) { return file->table(index, tag); };
    const sfnt::FontTables tables = sfnt::readFontTables(file->tableTags(index), fetch, platform::decodeLegacyText);
    return sharedToJs(env, sfnt::openTypeValue(tables, selection, fetch));
  } catch (const std::exception& e) {
    throw Napi::Error::New(env, std::string("readFontFile: ") + e.what());
  }
}

}  // namespace

// ---- module -----------------------------------------------------------------

Napi::Object initModule(Napi::Env env, Napi::Object exports) {
  exports.Set("getFamilies", Napi::Function::New(env, getFamilies, "getFamilies"));

  Napi::Object orthographies = Napi::Object::New(env);
  for (const orthography::Orthography& entry : orthography::all()) {
    orthographies.Set(std::string(entry.name), Napi::Number::New(env, entry.codePointCount));
  }
  exports.Set("orthographies", orthographies);

  Napi::Object testing = Napi::Object::New(env);
  testing.Set("registerFont", Napi::Function::New(env, registerFont, "registerFont"));
  testing.Set("unregisterFont", Napi::Function::New(env, unregisterFont, "unregisterFont"));
  testing.Set("getFamiliesWithWork", Napi::Function::New(env, getFamiliesWithWork, "getFamiliesWithWork"));
  testing.Set("readFontFile", Napi::Function::New(env, readFontFile, "readFontFile"));
  exports.Set("__testing", testing);
  return exports;
}

}  // namespace sfi

namespace {
Napi::Object initAddon(Napi::Env env, Napi::Object exports) { return sfi::initModule(env, exports); }
}  // namespace

NODE_API_MODULE(system_font_info, initAddon)
