#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

// The orthographies of fontconfig's fc-lang, built into the addon: for each language and script, the code points
// its writing system needs. Platform-independent; see README.md.

namespace sfi::orthography {

// Inclusive range of code points.
struct Range {
  uint32_t first;
  uint32_t last;
};

struct Orthography {
  std::string_view name;          // The fc-lang name, such as "zh-cn": lower case, words joined by '-'.
  std::span<const Range> ranges;  // Ascending, non-overlapping and non-adjacent.
  uint32_t codePointCount;        // The number of code points in `ranges`.
};

// The orthography of that exact name, or nullptr.
const Orthography* find(std::string_view name);

// Every orthography, ascending by name.
std::span<const Orthography> all();

// How many code points of `orthography` satisfy `has(codePoint)`.
template <class Has>
uint32_t countCovered(const Orthography& orthography, Has has) {
  uint32_t covered = 0;
  for (const Range& range : orthography.ranges) {
    for (uint32_t codePoint = range.first; codePoint <= range.last; ++codePoint) {
      if (has(codePoint)) ++covered;
    }
  }
  return covered;
}

namespace detail {
// Defined by the generated orthography_data.cpp.
extern const Orthography kOrthographies[];
extern const size_t kOrthographyCount;
}  // namespace detail

}  // namespace sfi::orthography
