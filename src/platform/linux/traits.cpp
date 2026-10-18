#include "traits.h"

#include <cstdlib>
#include <filesystem>

namespace sfi::platform::linux_backend {
namespace {
bool under(const std::string& path, const std::string& directory) {
  return !directory.empty() && path.size() > directory.size() && path.starts_with(directory) &&
         path[directory.size()] == '/';
}
}  // namespace

Scope scopeOf(const std::string& path) {
  if (under(path, "/usr/share/fonts") || under(path, "/usr/local/share/fonts")) return Scope::System;
  const char* home = std::getenv("HOME");
  const char* data = std::getenv("XDG_DATA_HOME");
  // XDG directories must be absolute; a relative value is ignored by the specification.
  std::filesystem::path dataDirectory;
  if (data && std::filesystem::path(data).is_absolute())
    dataDirectory = data;
  else if (home)
    dataDirectory = std::filesystem::path(home) / ".local/share";
  if ((!dataDirectory.empty() && under(path, (dataDirectory / "fonts").string())) ||
      (home && under(path, (std::filesystem::path(home) / ".fonts").string())))
    return Scope::User;
  return Scope::Unknown;
}

Format formatOf(const std::string& name) {
  if (name == "TrueType" || name == "Type 42") return Format::TrueType;
  if (name == "CFF") return Format::Cff;
  if (name == "Type 1" || name == "CID Type 1") return Format::Type1;
  if (name == "PCF" || name == "BDF" || name == "Windows FNT") return Format::Bitmap;
  return Format::Unknown;
}
}  // namespace sfi::platform::linux_backend
