#pragma once

#include <fontconfig/fontconfig.h>

namespace sfi::platform::linux_backend {
// Adds the process's test directory to this call's private configuration, if registered.
void addTestingFonts(FcConfig* config);
}  // namespace sfi::platform::linux_backend
