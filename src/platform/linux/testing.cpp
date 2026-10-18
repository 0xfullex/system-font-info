#include "testing.h"

#include <unistd.h>

#include <filesystem>
#include <functional>
#include <stdexcept>

#include "../../core/platform.h"
#include "fc.h"

namespace sfi::platform {
namespace {
using namespace linux_backend;
std::filesystem::path directory() {
  return std::filesystem::temp_directory_path() /
         ("system-font-info-test-" + std::to_string(getuid()) + "-" + std::to_string(getpid()));
}
std::filesystem::path linkFor(const std::string& path) {
  auto absolute = std::filesystem::absolute(std::filesystem::path(path)).lexically_normal();
  return directory() /
         (std::to_string(std::hash<std::string>{}(absolute.string())) + "-" + absolute.filename().string());
}
}  // namespace

void linux_backend::addTestingFonts(FcConfig* config) {
  auto path = directory();
  if (std::filesystem::exists(path))
    require(FcConfigAppFontAddDir(config, reinterpret_cast<const FcChar8*>(path.c_str())), "FcConfigAppFontAddDir");
}

void registerFontForTesting(const std::string& path) {
  Config validation(FcConfigCreate());
  require(bool(validation), "FcConfigCreate");
  require(FcConfigAppFontAddFile(validation.get(), reinterpret_cast<const FcChar8*>(path.c_str())),
          "FcConfigAppFontAddFile (test font validation)");
  auto fonts = FcConfigGetFonts(validation.get(), FcSetApplication);
  require(fonts && fonts->nfont > 0, "finding test font after FcConfigAppFontAddFile");
  auto link = linkFor(path);
  std::filesystem::create_directory(link.parent_path());
  auto target = std::filesystem::absolute(std::filesystem::path(path)).lexically_normal();
  if (std::filesystem::is_symlink(link)) {
    require(std::filesystem::read_symlink(link) == target, "resolving test font link collision");
    return;
  }
  std::filesystem::create_symlink(target, link);
}

void unregisterFontForTesting(const std::string& path) {
  require(std::filesystem::remove(linkFor(path)), "removing test font registration");
  auto pathDirectory = directory();
  if (std::filesystem::is_empty(pathDirectory)) std::filesystem::remove(pathDirectory);
}

// fontconfig lists a font or does not: it has no disabled state to turn on and off.
void setFontEnabledForTesting(const std::string&, bool) {
  throw std::runtime_error("system-font-info: this platform has no disabled fonts: setFontEnabled is not supported");
}
}  // namespace sfi::platform
