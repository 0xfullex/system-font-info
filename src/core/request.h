#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../orthography/orthography.h"
#include "../sfnt/sfnt.h"
#include "field_catalogue.h"

// What one call asks the native side to produce, and what it did. Plain standard C++20 (no Node-API): the JS object
// is parsed in addon.cpp, and the backends of every platform compile against this header.

namespace sfi {

using FieldSet = std::bitset<kFieldCount>;

// The platform this addon is built for.
#if defined(_WIN32)
inline constexpr Platform kCurrentPlatform = Platform::Win32;
#elif defined(__APPLE__)
inline constexpr Platform kCurrentPlatform = Platform::Darwin;
#else
inline constexpr Platform kCurrentPlatform = Platform::Linux;
#endif

// The request the JS layer has planned: the fields of the closure (ts/field-closure.ts), which are fields this
// platform fetches itself. The output (to_js.cpp) holds exactly what the request wants, so a backend may compute more
// than it is asked for but must never compute less. The one exception is `opentype`: to_js.cpp outputs the object the
// backend built whole, so a backend builds it from `openTypeSelection` (never `OpenTypeSelection::all()`), which is
// what keeps unrequested entries, and the errors of their tables, out of the result.
struct Request {
  // The requested fields that this platform provides. A field the platform derives is never in it (the JS layer
  // computes those), and neither is one the platform does not have.
  FieldSet fields;
  // Some `faces.platform.*` field was requested, whether or not this platform provides it: the faces get a `platform`
  // object, holding `os` and the requested platform fields.
  bool platformObject = false;
  // The family names to return, matched exactly and case-sensitively; unset for every family, empty for none.
  std::optional<std::vector<std::string>> families;
  // The orthographies whose coverage is counted, in the order of `Family::languageCoverage`.
  std::vector<const orthography::Orthography*> orthographies;
  // The `faces.opentype.*` entries requested. A backend passes it to `sfnt::openTypeValue` and attaches the value as
  // it is: the conversion does not filter an `opentype` object again.
  sfnt::OpenTypeSelection openTypeSelection;

  bool wants(Field field) const { return fields.test(size_t(field)); }
  // Some face-level field (any of the faces, opentype and platform levels) was requested: the families get `faces`
  // and their members have to be listed and put in the platform's order.
  bool wantsAnyFace() const;
  // Some `faces.opentype.*` entry was requested.
  bool wantsAnyOpenType() const { return openTypeSelection.entries.any(); }
  // Whether `family` is one of the requested families: the one place that decides it, for every backend.
  bool wantsFamily(std::string_view family) const;
  // The groups of OS work (docs/platforms.md, "Source groups") the requested fields always need on this platform.
  // Group A is always needed: every call lists the families.
  bool needs(Group group) const;
  // Whether the group may be needed: `needs`, or a requested field needs it in some cases.
  bool mayNeed(Group group) const;
};

// What a call did, per kind of costly OS work (groups C, D and E). Groups A and B are not counted: they are cheap, and
// the line between them is not observable on every platform. Tests read it through `__testing.getFamiliesWithWork`.
struct WorkLog {
  uint32_t fontResources = 0;  // C: font objects created or font files opened to read a face
  uint32_t tableEntries = 0;   // D: `opentype` entries built
  uint32_t characterSets = 0;  // E: character sets fetched and walked
};

// Builds the request from the names the JS layer sends. Throws std::invalid_argument for a name that is not a field
// this platform fetches itself or not an orthography: the JS layer has validated the caller's input by then, so
// this is an internal error.
Request makeRequest(const std::vector<std::string>& fieldNames, std::optional<std::vector<std::string>> families,
                    const std::vector<std::string>& orthographyNames);

}  // namespace sfi
