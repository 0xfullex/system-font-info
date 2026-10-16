#include <stdexcept>
#include <string>

#include "../../core/platform.h"
#include "cf.h"

// Test hooks: register a font file for the current process only, the scope
// whose fonts disappear with the process and are visible to no other app.

namespace sfi::platform {
namespace {

using darwin::Ref;

Ref<CFURLRef> fileUrl(const std::string& path) {
  const auto cfPath = darwin::makeString(path);
  return Ref<CFURLRef>(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfPath.get(), kCFURLPOSIXPathStyle, false));
}

[[noreturn]] void throwFailure(const char* action, const std::string& path, CFErrorRef error) {
  Ref<CFStringRef> description(error ? CFErrorCopyDescription(error) : nullptr);
  throw std::runtime_error(std::string("system-font-info: could not ") + action + " " + path + ": " +
                           (description ? darwin::toUtf8(description.get()) : "unknown error"));
}

}  // namespace

void registerFontForTesting(const std::string& path) {
  const auto url = fileUrl(path);
  CFErrorRef error = nullptr;
  if (!CTFontManagerRegisterFontsForURL(url.get(), kCTFontManagerScopeProcess, &error)) {
    Ref<CFErrorRef> owned(error);
    throwFailure("register", path, error);
  }
}

void unregisterFontForTesting(const std::string& path) {
  const auto url = fileUrl(path);
  CFErrorRef error = nullptr;
  if (!CTFontManagerUnregisterFontsForURL(url.get(), kCTFontManagerScopeProcess, &error)) {
    Ref<CFErrorRef> owned(error);
    throwFailure("unregister", path, error);
  }
}

}  // namespace sfi::platform
