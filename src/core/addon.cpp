#include <napi.h>

#include <cmath>
#include <exception>
#include <string>
#include <vector>

#include "../sfnt/sfnt.h"
#include "model.h"
#include "platform.h"
#include "to_js.h"

namespace sfi {
namespace {

// ---- getFonts -------------------------------------------------------------

EnumerateOptions parseEnumerateOptions(const Napi::CallbackInfo& info) {
  EnumerateOptions options;
  if (info.Length() == 0 || info[0].IsUndefined() || info[0].IsNull()) return options;
  if (!info[0].IsObject()) throw Napi::TypeError::New(info.Env(), "getFonts: options must be an object");
  Napi::Object object = info[0].As<Napi::Object>();
  for (auto [key, target] : {std::pair<const char*, bool*>{"tables", &options.tables},
                             std::pair<const char*, bool*>{"coverage", &options.coverage}}) {
    Napi::Value value = object.Get(key);
    if (value.IsUndefined()) continue;
    if (!value.IsBoolean()) {
      throw Napi::TypeError::New(info.Env(), std::string("getFonts: options.") + key + " must be a boolean");
    }
    *target = value.As<Napi::Boolean>().Value();
  }
  return options;
}

// Enumerates on a worker thread; the result is converted and the promise
// settled back on the JS thread.
class GetFontsWorker : public Napi::AsyncWorker {
 public:
  GetFontsWorker(Napi::Env env, EnumerateOptions options)
      : Napi::AsyncWorker(env, "system-font-info:getFonts"),
        deferred_(Napi::Promise::Deferred::New(env)),
        options_(options) {}

  Napi::Promise promise() const { return deferred_.Promise(); }

 protected:
  void Execute() override {
    try {
      families_ = platform::enumerateFonts(options_);
    } catch (const std::exception& e) {
      SetError(e.what());
    } catch (...) {
      SetError("system-font-info: unknown error while listing fonts");
    }
  }

  void OnOK() override {
    Napi::Env env = Env();
    try {
      Napi::Array result = familiesToJs(env, families_);
      families_.clear();
      deferred_.Resolve(result);
    } catch (const Napi::Error& e) {
      deferred_.Reject(e.Value());
    }
  }

  void OnError(const Napi::Error& error) override { deferred_.Reject(error.Value()); }

 private:
  Napi::Promise::Deferred deferred_;
  EnumerateOptions options_;
  std::vector<Family> families_;
};

Napi::Value getFonts(const Napi::CallbackInfo& info) {
  auto* worker = new GetFontsWorker(info.Env(), parseEnumerateOptions(info));
  Napi::Promise promise = worker->promise();
  worker->Queue();  // the worker deletes itself after OnOK / OnError
  return promise;
}

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
// backends use, and returns its `opentype` object.
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

  try {
    sfnt::FontTables tables = sfnt::readFontTables(
        file->tableTags(index), [&](uint32_t tag) { return file->table(index, tag); }, platform::decodeLegacyName);
    return sharedToJs(env, tables.opentype);
  } catch (const std::exception& e) {
    throw Napi::Error::New(env, std::string("readFontFile: ") + e.what());
  }
}

}  // namespace

// ---- module -----------------------------------------------------------------

Napi::Object initModule(Napi::Env env, Napi::Object exports) {
  exports.Set("getFonts", Napi::Function::New(env, getFonts, "getFonts"));

  Napi::Object testing = Napi::Object::New(env);
  testing.Set("registerFont", Napi::Function::New(env, registerFont, "registerFont"));
  testing.Set("unregisterFont", Napi::Function::New(env, unregisterFont, "unregisterFont"));
  testing.Set("readFontFile", Napi::Function::New(env, readFontFile, "readFontFile"));
  exports.Set("__testing", testing);
  return exports;
}

}  // namespace sfi

namespace {
Napi::Object initAddon(Napi::Env env, Napi::Object exports) { return sfi::initModule(env, exports); }
}  // namespace

NODE_API_MODULE(system_font_info, initAddon)
