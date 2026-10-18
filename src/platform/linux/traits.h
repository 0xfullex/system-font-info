#pragma once

#include <string>

#include "../../core/model.h"

namespace sfi::platform::linux_backend {
Scope scopeOf(const std::string& path);
Format formatOf(const std::string& name);
}  // namespace sfi::platform::linux_backend
