#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "cf.h"

namespace sfi::platform::darwin {

// The code points a font covers (kCTFontCharacterSetAttribute) as ascending,
// inclusive ranges. Empty when CoreText gives no character set.
std::vector<std::pair<uint32_t, uint32_t>> coverageRanges(CTFontDescriptorRef descriptor);

}  // namespace sfi::platform::darwin
