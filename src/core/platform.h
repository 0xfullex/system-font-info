#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "../sfnt/legacy_encoding.h"
#include "model.h"
#include "request.h"

// The interface each platform backend implements (src/platform/darwin,
// src/platform/win32, src/platform/linux). Exactly one backend is compiled in.

namespace sfi::platform {

// Lists the installed fonts through the OS font service. Runs on a worker
// thread, never on the JS thread. Throws std::exception on failure.
//
// Returns only the families for which `request.wantsFamily` is true. Fills what
// the request wants (`request.wants`, `wantsAnyOpenType`, the orthographies);
// the conversion to JS outputs only that, so a backend may fill more but never
// less. The exception is `Face::opentype`, which the conversion outputs as the
// backend built it: build it from `request.openTypeSelection` and never from
// `OpenTypeSelection::all()`, or the result holds entries (and table errors)
// the caller did not select. Adds the costly OS work it does (groups C, D and E)
// to `work`.
std::vector<Family> enumerateFonts(const Request& request, WorkLog& work);

// Converts bytes in a legacy character encoding (a `name` table string that is
// not UTF-16BE) to UTF-8, with the platform's own converter. Which encoding a
// record uses is decided by sfnt, never here: this only converts. Returns
// nullopt when the platform has no converter for `encoding` or the bytes do not
// convert. Thread-safe.
std::optional<std::string> decodeLegacyText(sfnt::LegacyEncoding encoding, std::span<const uint8_t> bytes);

// Test hooks: make a font file available to the current process only, through
// the same OS service enumeration reads from, and take it away again. The
// caller keeps the file in place until it has unregistered it: a platform may
// read the file again on every enumeration, so a file removed while registered
// can break the calls that follow.
void registerFontForTesting(const std::string& path);
void unregisterFontForTesting(const std::string& path);

// Test hook: turns the fonts of a file registered for the current process off or
// on again, as the user does in a font manager, for the current process only.
// Throws on a platform without a disabled state, and when asked to turn off a file
// that is not registered for the current process (the user's fonts stay untouched).
void setFontEnabledForTesting(const std::string& path, bool enabled);

}  // namespace sfi::platform
