#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include "../../core/model.h"
#include "cf.h"

// Conversions from CoreText's own scales and codes to the cross-platform
// fields of `Face`. The functions are pure, so they are the part of the
// backend that can be checked without a font.

namespace sfi::platform::darwin {

// The entries of kCTFontTraitsAttribute; an entry the descriptor lacks is unset.
struct Traits {
  std::optional<double> weight;      // kCTFontWeightTrait, -1...1
  std::optional<double> width;       // kCTFontWidthTrait, -1...1
  std::optional<double> slant;       // kCTFontSlantTrait, -1...1
  std::optional<uint32_t> symbolic;  // kCTFontSymbolicTrait, class bits included
};

Traits readTraits(CTFontDescriptorRef descriptor);

// CoreText weight trait -> CSS font-weight, with the same bucket boundaries as
// Chromium's mapping for macOS. A browser that picks a face by CSS weight
// should pick the same face this library reports for that weight, which is
// why the boundaries are copied rather than derived from the nominal weight
// values of the system fonts. A missing trait is the neutral 0 (CSS 400).
double cssWeight(std::optional<double> coreTextWeight);

// CoreText width trait -> CSS font-stretch percentage (50-200). A missing
// trait is the neutral 0 (100).
double cssWidth(std::optional<double> coreTextWidth);

// The italic symbolic bit makes a face italic, anything else is normal, which
// is how a browser classifies a macOS face. The slant trait is not used: it is
// non-zero for upright script faces (Zapfino) and does not mean the face is
// slanted text. It stays available as `platform.slant`.
Style styleOf(const Traits& traits);

// kCTFontPriorityAttribute and kCTFontRegistrationScopeAttribute -> who
// installed the font; the rules are in the README: the process registration
// scope is `process`, and otherwise the priority decides (the user and the
// dynamic priorities are `user`). Either may be absent.
Scope scopeOf(std::optional<int64_t> priority, std::optional<int64_t> registrationScope);

// kCTFontFormatAttribute -> Format. CoreText reports CFF and CFF2 as one
// format, OpenType PostScript; only for that format `hasCff2` is called, to tell
// them apart, so a caller that has to open the font for it does so only then.
Format formatOf(std::optional<int64_t> coreTextFormat, const std::function<bool()>& hasCff2);

}  // namespace sfi::platform::darwin
