#include <condition_variable>
#include <mutex>
#include <set>
#include <thread>

#include "../../core/platform.h"
#include "cf.h"

// watchFonts: font-set change notifications.
//
// CoreText posts kCTFontManagerRegisteredFontsChangedNotification when fonts
// are registered or unregistered. Apple's header asks clients to listen on the
// distributed center for session and persistent scopes and on the local center
// for process scope, so both are observed. Neither is delivered by a thread of
// our own: CoreText posts from the main thread's run loop, so the callback runs
// there (see README.md for what was measured). A plain Node process never
// services that run loop, which is why this backend does not depend on being
// able to run anything on the main thread: it owns a thread with a run loop
// whose only job is to host the observers and stay alive.

namespace sfi::platform {
namespace {

// Watchers that are still running. A notification callback holds no
// reference to its watcher, only its address, and looks it up here under the
// lock, so a callback that races with the destructor either finishes before
// the destructor returns or finds the watcher gone and does nothing.
std::mutex& registryMutex() {
  static std::mutex mutex;
  return mutex;
}

class RunLoopWatcher;
std::set<RunLoopWatcher*>& registry() {
  static std::set<RunLoopWatcher*> watchers;
  return watchers;
}

class RunLoopWatcher final : public FontWatcher {
 public:
  explicit RunLoopWatcher(std::function<void()> onChange) : onChange_(std::move(onChange)) {
    {
      std::lock_guard lock(registryMutex());
      registry().insert(this);
    }
    thread_ = std::thread([this] { run(); });
    std::unique_lock lock(readyMutex_);
    ready_.wait(lock, [this] { return loop_ != nullptr; });
  }

  ~RunLoopWatcher() override {
    {
      // After this block no callback calls onChange_ any more.
      std::lock_guard lock(registryMutex());
      registry().erase(this);
    }
    // The stop source also wakes the run loop; signalling it before the loop
    // has started makes the loop stop as soon as it does, so the join below
    // cannot hang on a stop request that arrived too early.
    CFRunLoopSourceSignal(stopSource_.get());
    CFRunLoopWakeUp(loop_);
    thread_.join();
  }

  static void notified(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef) {
    std::lock_guard lock(registryMutex());
    auto* watcher = static_cast<RunLoopWatcher*>(observer);
    if (registry().count(watcher)) watcher->onChange_();
  }

 private:
  static void stopRunLoop(void*) { CFRunLoopStop(CFRunLoopGetCurrent()); }

  void run() {
    CFRunLoopSourceContext context{};
    context.perform = stopRunLoop;
    stopSource_ = darwin::Ref<CFRunLoopSourceRef>(CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context));
    // A run loop with no source or timer returns at once. The stop source is
    // in the loop for its whole life, which keeps it running.
    CFRunLoopAddSource(CFRunLoopGetCurrent(), stopSource_.get(), kCFRunLoopDefaultMode);

    const CFNotificationCenterRef centers[] = {CFNotificationCenterGetLocalCenter(),
                                               CFNotificationCenterGetDistributedCenter()};
    for (CFNotificationCenterRef center : centers) {
      CFNotificationCenterAddObserver(center, this, &RunLoopWatcher::notified,
                                      kCTFontManagerRegisteredFontsChangedNotification, nullptr,
                                      CFNotificationSuspensionBehaviorDeliverImmediately);
    }

    {
      std::lock_guard lock(readyMutex_);
      loop_ = CFRunLoopGetCurrent();
    }
    ready_.notify_one();

    CFRunLoopRun();

    for (CFNotificationCenterRef center : centers) CFNotificationCenterRemoveEveryObserver(center, this);
    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), stopSource_.get(), kCFRunLoopDefaultMode);
  }

  std::function<void()> onChange_;
  std::thread thread_;
  std::mutex readyMutex_;
  std::condition_variable ready_;
  CFRunLoopRef loop_ = nullptr;  // the watcher thread's run loop; set once, under readyMutex_
  darwin::Ref<CFRunLoopSourceRef> stopSource_;
};

}  // namespace

std::unique_ptr<FontWatcher> watchFonts(std::function<void()> onChange) {
  return std::make_unique<RunLoopWatcher>(std::move(onChange));
}

}  // namespace sfi::platform
