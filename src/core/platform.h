#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "model.h"

// The interface each platform backend implements (src/platform/darwin,
// src/platform/win32, src/platform/linux). Exactly one backend is compiled in.

namespace sfi::platform {

// Lists the installed fonts through the OS font service. Runs on a worker
// thread, never on the JS thread. Throws std::exception on failure.
std::vector<Family> enumerateFonts(const EnumerateOptions& options);

// A running subscription to OS font-set changes. Destroying it stops the
// subscription and joins any thread it owns; no callback runs afterwards.
class FontWatcher {
 public:
  virtual ~FontWatcher() = default;
};

// Starts watching. `onChange` may be called from any thread, may be called
// more than once per change, and carries no detail about what changed.
std::unique_ptr<FontWatcher> watchFonts(std::function<void()> onChange);

// Decodes a `name` table string stored in a legacy (non-UTF-16) encoding, such
// as Mac platform records in MacRoman or MacChineseSimp. Returns nullopt when
// the platform cannot decode that encoding. Thread-safe.
std::optional<std::string> decodeLegacyName(uint16_t platformId, uint16_t encodingId, uint16_t languageId,
                                            std::span<const uint8_t> bytes);

// Test hooks: make a font file available to the current process only, through
// the same OS service enumeration reads from, and take it away again.
void registerFontForTesting(const std::string& path);
void unregisterFontForTesting(const std::string& path);

}  // namespace sfi::platform
