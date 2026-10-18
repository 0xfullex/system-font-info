#include "testing.h"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <stdexcept>

#include "../../core/platform.h"
#include "dwrite.h"

namespace sfi::platform {
namespace {

std::mutex registrationMutex;
std::vector<std::wstring> registrations;

std::wstring absolutePath(const std::string& path) {
  return std::filesystem::absolute(std::filesystem::path(win32::toWide(path))).lexically_normal().wstring();
}

}  // namespace

namespace win32 {

std::vector<std::wstring> registeredFonts() {
  const std::lock_guard lock(registrationMutex);
  return registrations;
}

}  // namespace win32

void registerFontForTesting(const std::string& path) {
  const auto absolute = absolutePath(path);
  // Validate with the same API each enumeration uses, without installing a font or changing the registry.
  const auto factory = win32::createFactory();
  win32::ComPtr<IDWriteFontSetBuilder2> builder;
  win32::check(factory->CreateFontSetBuilder(&builder), "CreateFontSetBuilder");
  win32::check(builder->AddFontFile(absolute.c_str()), "AddFontFile");
  const std::lock_guard lock(registrationMutex);
  if (std::find(registrations.begin(), registrations.end(), absolute) != registrations.end())
    throw std::runtime_error("system-font-info: test font is already registered: " + path);
  registrations.push_back(absolute);
}

void unregisterFontForTesting(const std::string& path) {
  const auto absolute = absolutePath(path);
  const std::lock_guard lock(registrationMutex);
  const auto found = std::find(registrations.begin(), registrations.end(), absolute);
  if (found == registrations.end()) throw std::runtime_error("system-font-info: test font is not registered: " + path);
  registrations.erase(found);
}

// DirectWrite has no disabled state to turn a font into, so this is never supported.
void setFontEnabledForTesting(const std::string&, bool) {
  throw std::runtime_error("system-font-info: this platform has no disabled fonts: setFontEnabled is not supported");
}

}  // namespace sfi::platform
