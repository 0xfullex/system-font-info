#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

// Small CoreFoundation helpers shared by the darwin backend: an owning
// reference, checked casts and value extraction. Attribute dictionaries from
// CoreText are loosely typed (a number may be a CFNumber or a CFBoolean, a
// value may be missing), so every accessor checks the type and reports absence
// instead of assuming.

namespace sfi::platform::darwin {

// Owns one retain count of a CF object obtained from a Create or Copy call.
template <typename T>
class Ref {
 public:
  Ref() = default;
  explicit Ref(T ref) : ref_(ref) {}
  Ref(const Ref&) = delete;
  Ref& operator=(const Ref&) = delete;
  Ref(Ref&& other) noexcept : ref_(std::exchange(other.ref_, nullptr)) {}
  Ref& operator=(Ref&& other) noexcept {
    if (this != &other) {
      reset();
      ref_ = std::exchange(other.ref_, nullptr);
    }
    return *this;
  }
  ~Ref() { reset(); }

  T get() const { return ref_; }
  explicit operator bool() const { return ref_ != nullptr; }

 private:
  void reset() {
    if (ref_) CFRelease(ref_);
    ref_ = nullptr;
  }
  T ref_ = nullptr;
};

inline CFStringRef asString(CFTypeRef value) {
  return value && CFGetTypeID(value) == CFStringGetTypeID() ? static_cast<CFStringRef>(value) : nullptr;
}
inline CFNumberRef asNumber(CFTypeRef value) {
  return value && CFGetTypeID(value) == CFNumberGetTypeID() ? static_cast<CFNumberRef>(value) : nullptr;
}
inline CFArrayRef asArray(CFTypeRef value) {
  return value && CFGetTypeID(value) == CFArrayGetTypeID() ? static_cast<CFArrayRef>(value) : nullptr;
}
inline CFDictionaryRef asDictionary(CFTypeRef value) {
  return value && CFGetTypeID(value) == CFDictionaryGetTypeID() ? static_cast<CFDictionaryRef>(value) : nullptr;
}
inline CFURLRef asUrl(CFTypeRef value) {
  return value && CFGetTypeID(value) == CFURLGetTypeID() ? static_cast<CFURLRef>(value) : nullptr;
}

// UTF-8 text of a CFString; "" for null.
std::string toUtf8(CFStringRef string);

Ref<CFStringRef> makeString(const std::string& utf8);

// Numbers. A CFBoolean is accepted where a number is asked for (CoreText is
// not consistent about which one it stores); anything else yields nullopt.
std::optional<double> numberValue(CFTypeRef value);
std::optional<int64_t> integerValue(CFTypeRef value);
// A CFBoolean, or a CFNumber read as "non-zero".
std::optional<bool> booleanValue(CFTypeRef value);

// The value stored under a string key of a CF dictionary, or null.
inline CFTypeRef dictionaryValue(CFDictionaryRef dictionary, CFStringRef key) {
  return dictionary ? CFDictionaryGetValue(dictionary, key) : nullptr;
}

inline Ref<CFTypeRef> copyAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  return Ref<CFTypeRef>(CTFontDescriptorCopyAttribute(descriptor, attribute));
}

// The POSIX path of a file URL, or "" when `value` is not a file URL.
std::string filePath(CFTypeRef url);

}  // namespace sfi::platform::darwin
