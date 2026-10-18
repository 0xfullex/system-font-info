#pragma once

#include <string>
#include <vector>

namespace sfi::platform::win32 {

// Test registrations are the only process state; production enumerations retain no font service objects or caches.
std::vector<std::wstring> registeredFonts();

}  // namespace sfi::platform::win32
