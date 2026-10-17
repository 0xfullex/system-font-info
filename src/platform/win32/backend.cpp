#include <stdexcept>

#include "../../core/platform.h"

// The Windows backend is not written yet. Every entry point fails with a clear
// message instead of returning an empty result, so a missing backend is never
// mistaken for a machine without fonts.

namespace sfi::platform {
namespace {

[[noreturn]] void notImplemented() {
  throw std::runtime_error("system-font-info: the win32 backend is not implemented yet");
}

}  // namespace

std::vector<Family> enumerateFonts(const EnumerateOptions&) { notImplemented(); }

std::optional<std::string> decodeLegacyName(uint16_t, uint16_t, uint16_t, std::span<const uint8_t>) {
  return std::nullopt;
}

void registerFontForTesting(const std::string&) { notImplemented(); }

void unregisterFontForTesting(const std::string&) { notImplemented(); }

}  // namespace sfi::platform
