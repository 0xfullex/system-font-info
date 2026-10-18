#pragma once

#include <fontconfig/fontconfig.h>

namespace sfi::platform::linux_backend {
// Adds the fonts registered for this process to this call's private configuration as its application fonts.
void addTestingFonts(FcConfig* config);
}  // namespace sfi::platform::linux_backend
