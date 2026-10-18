#pragma once

#include <string>

#include "../../core/model.h"

namespace sfi::platform::linux_backend {
// `inApplicationSet`: the font comes from the call's own application font set, which only the test hook fills.
Scope scopeOf(const std::string& path, bool inApplicationSet);
Format formatOf(const std::string& name);
}  // namespace sfi::platform::linux_backend
