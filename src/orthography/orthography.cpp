#include "orthography.h"

#include <algorithm>

namespace sfi::orthography {

std::span<const Orthography> all() { return {detail::kOrthographies, detail::kOrthographyCount}; }

const Orthography* find(std::string_view name) {
  const std::span<const Orthography> table = all();
  const auto it =
      std::lower_bound(table.begin(), table.end(), name,
                       [](const Orthography& orthography, std::string_view key) { return orthography.name < key; });
  return it != table.end() && it->name == name ? &*it : nullptr;
}

}  // namespace sfi::orthography
