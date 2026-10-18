#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../orthography/orthography.h"
#include "cf.h"

namespace sfi::platform::darwin {

// The default face of a family, which `languageCoverage` is counted on: what
// CoreText's matcher returns for the family name as a mandatory attribute and
// the weight and width traits 0 (regular weight, normal width). Null when the
// matcher returns nothing, or a face of another family (it may return one when
// the family has no face it can match, for example when all its faces are
// disabled).
Ref<CTFontDescriptorRef> matchDefaultFace(const std::string& family);

// How many code points of each orthography the face covers
// (kCTFontCharacterSetAttribute), in the order of `orthographies`. All zero for
// a face without a character set.
std::vector<uint32_t> countLanguageCoverage(CTFontDescriptorRef face,
                                            const std::vector<const orthography::Orthography*>& orthographies);

}  // namespace sfi::platform::darwin
