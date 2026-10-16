#include "cf.h"

#include <vector>

namespace sfi::platform::darwin {

std::string toUtf8(CFStringRef string) {
  if (!string) return {};
  if (const char* direct = CFStringGetCStringPtr(string, kCFStringEncodingUTF8)) return direct;
  const CFIndex length = CFStringGetLength(string);
  const CFIndex capacity = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
  std::string out(static_cast<size_t>(capacity), '\0');
  CFIndex used = 0;
  CFStringGetBytes(string, CFRangeMake(0, length), kCFStringEncodingUTF8, 0, false,
                   reinterpret_cast<UInt8*>(out.data()), capacity, &used);
  out.resize(static_cast<size_t>(used));
  return out;
}

Ref<CFStringRef> makeString(const std::string& utf8) {
  return Ref<CFStringRef>(CFStringCreateWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(utf8.data()),
                                                  static_cast<CFIndex>(utf8.size()), kCFStringEncodingUTF8, false));
}

std::optional<double> numberValue(CFTypeRef value) {
  if (value && CFGetTypeID(value) == CFBooleanGetTypeID()) return CFBooleanGetValue(static_cast<CFBooleanRef>(value)) ? 1.0 : 0.0;
  CFNumberRef number = asNumber(value);
  double result = 0;
  if (!number || !CFNumberGetValue(number, kCFNumberDoubleType, &result)) return std::nullopt;
  return result;
}

std::optional<int64_t> integerValue(CFTypeRef value) {
  if (value && CFGetTypeID(value) == CFBooleanGetTypeID()) return CFBooleanGetValue(static_cast<CFBooleanRef>(value)) ? 1 : 0;
  CFNumberRef number = asNumber(value);
  int64_t result = 0;
  if (!number || !CFNumberGetValue(number, kCFNumberSInt64Type, &result)) return std::nullopt;
  return result;
}

std::optional<bool> booleanValue(CFTypeRef value) {
  const auto number = integerValue(value);
  if (!number) return std::nullopt;
  return *number != 0;
}

std::string filePath(CFTypeRef url) {
  CFURLRef fileUrl = asUrl(url);
  if (!fileUrl) return {};
  Ref<CFStringRef> path(CFURLCopyFileSystemPath(fileUrl, kCFURLPOSIXPathStyle));
  return toUtf8(path.get());
}

}  // namespace sfi::platform::darwin
