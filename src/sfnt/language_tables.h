#pragma once

#include <cstddef>
#include <cstdint>

// Generated data (language_tables.cpp, by scripts/gen-language-tables.c).

namespace sfi::sfnt {

struct LcidEntry {
  uint16_t lcid;
  const char* tag;
};
struct MacLanguageEntry {
  uint16_t code;
  const char* tag;
};

extern const LcidEntry kLcidTable[];  // ascending by lcid
extern const size_t kLcidTableSize;
extern const MacLanguageEntry kMacLanguageTable[];  // ascending by code
extern const size_t kMacLanguageTableSize;

}  // namespace sfi::sfnt
