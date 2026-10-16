#include <napi.h>

#include <atomic>
#include <cmath>
#include <exception>
#include <memory>
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
      : Napi::AsyncWorker(env, "system-font-info:getFonts"), deferred_(Napi::Promise::Deferred::New(env)), options_(options) {}

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

// ---- watching ---------------------------------------------------------------

// One startWatching .. stopWatching span.
struct WatchSession {
  Napi::ThreadSafeFunction callback;
  // Set when a call is queued and not yet delivered; further notifications are
  // dropped meanwhile. Cleared on the JS thread right before `cb()` runs.
  std::atomic<bool> queued{false};
  // Set on the JS thread when the session ends, so a call that was already
  // queued does not reach `cb` afterwards.
  std::atomic<bool> stopped{false};
  std::unique_ptr<platform::FontWatcher> watcher;
};

// Calls `cb()`. A JS exception thrown by `cb` stays pending and is reported by
// Node as usual. Any other failure means the environment is shutting down and
// can no longer run JS, in which case the notification is moot.
void callWithoutArguments(Napi::Env env, const Napi::Function& cb) {
  napi_value undefined;
  napi_get_undefined(env, &undefined);
  napi_call_function(env, undefined, cb, 0, nullptr, nullptr);
}

// Per-environment state (one native watcher per env).
class AddonData {
 public:
  ~AddonData() { stopWatching(); }

  bool watching() const { return session_ != nullptr; }

  void startWatching(Napi::Env env, Napi::Function callback) {
    auto session = std::make_shared<WatchSession>();
    session->callback = Napi::ThreadSafeFunction::New(env, callback, "system-font-info:watch", 0, 1);
    // Watching must never keep the process alive.
    session->callback.Unref(env);
    try {
      session->watcher = platform::watchFonts([session] {
        if (session->queued.exchange(true)) return;
        napi_status status = session->callback.NonBlockingCall([session](Napi::Env env, Napi::Function cb) {
          session->queued = false;
          if (!session->stopped) callWithoutArguments(env, cb);
        });
        if (status != napi_ok) session->queued = false;
      });
    } catch (const std::exception& e) {
      session->stopped = true;
      session->callback.Release();
      throw Napi::Error::New(env, e.what());
    }
    session_ = std::move(session);
  }

  // Destroying the watcher joins its thread, so no notification arrives after
  // it; only then is the callback released. A no-op when not watching.
  void stopWatching() {
    if (!session_) return;
    std::shared_ptr<WatchSession> session = std::move(session_);
    session->stopped = true;
    session->watcher.reset();
    session->callback.Release();
  }

 private:
  std::shared_ptr<WatchSession> session_;
};

Napi::Value startWatching(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  auto* data = static_cast<AddonData*>(info.Data());
  if (info.Length() < 1 || !info[0].IsFunction()) throw Napi::TypeError::New(env, "startWatching: callback must be a function");
  if (data->watching()) throw Napi::Error::New(env, "startWatching: already watching; call stopWatching first");
  data->startWatching(env, info[0].As<Napi::Function>());
  return env.Undefined();
}

Napi::Value stopWatching(const Napi::CallbackInfo& info) {
  static_cast<AddonData*>(info.Data())->stopWatching();
  return info.Env().Undefined();
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
    throw Napi::RangeError::New(env, "readFontFile: face index " + info[1].ToString().Utf8Value() + " is out of range; '" + path +
                                         "' has " + std::to_string(file->faceCount()) + " face(s)");
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
  auto* data = new AddonData();
  env.SetInstanceData(data);
  // Teardown (process exit, worker or renderer shutdown) must join the watcher
  // thread before the environment goes away.
  env.AddCleanupHook([data] { data->stopWatching(); });

  exports.Set("getFonts", Napi::Function::New(env, getFonts, "getFonts"));
  exports.Set("startWatching", Napi::Function::New(env, startWatching, "startWatching", data));
  exports.Set("stopWatching", Napi::Function::New(env, stopWatching, "stopWatching", data));

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
