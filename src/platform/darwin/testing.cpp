#include <stdexcept>
#include <string>

#include "../../core/platform.h"
#include "cf.h"

// Test hooks: register a font file for the current process only, the scope
// whose fonts disappear with the process and are visible to no other app, and
// disable or enable the fonts of such a file.

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

void setFontEnabledForTesting(const std::string& path, bool enabled) {
  const auto url = fileUrl(path);
  // Disabling a font that is registered beyond this process is a change to the user's fonts that outlives the test, so
  // only a font registered for the process is disabled. Enabling cannot be guarded the same way: the file of a disabled
  // font reports scope 0, the same as an unregistered file or one disabled for the user, so no check separates the
  // fonts this hook disabled from the others. Enabling only undoes a disable, and a disable cannot reach other fonts.
  if (!enabled && CTFontManagerGetScopeForURL(url.get()) != kCTFontManagerScopeProcess) {
    throw std::runtime_error("system-font-info: " + path +
                             " is not registered for this process, so it is not disabled");
  }
  const Ref<CFArrayRef> descriptors(CTFontManagerCreateFontDescriptorsFromURL(url.get()));
  if (!descriptors || CFArrayGetCount(descriptors.get()) == 0) {
    throw std::runtime_error("system-font-info: " + path + " holds no fonts to " + (enabled ? "enable" : "disable"));
  }
  // The call reports nothing; the font is off or on in this process only when it was registered for the process.
  CTFontManagerEnableFontDescriptors(descriptors.get(), enabled);
}

}  // namespace sfi::platform
