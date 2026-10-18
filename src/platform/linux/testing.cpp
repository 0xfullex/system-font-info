#include "testing.h"

#include <algorithm>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <vector>

#include "../../core/platform.h"
#include "fc.h"

namespace sfi::platform {
namespace {
using namespace linux_backend;

// The files registered for this process. They live in memory only: any state outside the process, such as a
// directory in /tmp, could be pre-created by another local user or left behind by a crashed test, and a real call
// would then list its contents.
std::mutex registryMutex;
std::vector<std::string> registry;

std::string normalized(const std::string& path) {
  return std::filesystem::absolute(std::filesystem::path(path)).lexically_normal().string();
}
}  // namespace

void linux_backend::addTestingFonts(FcConfig* config) {
  std::vector<std::string> paths;
  {
    std::lock_guard lock(registryMutex);
    paths = registry;
  }
  for (const auto& path : paths)
    require(FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(path.c_str())),
            "FcConfigAppFontAddFile (registered test font)");
}

void registerFontForTesting(const std::string& path) {
  const auto file = normalized(path);
  Config validation(FcConfigCreate());
  require(bool(validation), "FcConfigCreate");
  require(FcConfigAppFontAddFile(validation.get(), reinterpret_cast<const FcChar8*>(file.c_str())),
          "FcConfigAppFontAddFile (test font validation)");
  auto fonts = FcConfigGetFonts(validation.get(), FcSetApplication);
  require(fonts && fonts->nfont > 0, "finding test font after FcConfigAppFontAddFile");
  std::lock_guard lock(registryMutex);
  if (std::find(registry.begin(), registry.end(), file) == registry.end()) registry.push_back(file);
}

void unregisterFontForTesting(const std::string& path) {
  const auto file = normalized(path);
  std::lock_guard lock(registryMutex);
  auto found = std::find(registry.begin(), registry.end(), file);
  require(found != registry.end(), "finding the test font registration");
  registry.erase(found);
}

// fontconfig lists a font or does not: it has no disabled state to turn on and off.
void setFontEnabledForTesting(const std::string&, bool) {
  throw std::runtime_error("system-font-info: this platform has no disabled fonts: setFontEnabled is not supported");
}
}  // namespace sfi::platform
