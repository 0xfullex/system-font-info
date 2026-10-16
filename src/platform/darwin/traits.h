#pragma once

#include <cstdint>
#include <optional>

#include "../../core/model.h"
#include "cf.h"

// Conversions from CoreText's own scales and codes to the cross-platform
// fields of `Face`. The functions are pure, so they are the part of the
// backend that can be checked without a font.

namespace sfi::platform::darwin {

struct Traits {
  double weight = 0;  // kCTFontWeightTrait, -1...1
  double width = 0;   // kCTFontWidthTrait, -1...1
  double slant = 0;   // kCTFontSlantTrait, -1...1
  uint32_t symbolic = 0;  // kCTFontSymbolicTrait, class bits included
};

// Reads kCTFontTraitsAttribute; absent entries stay 0.
Traits readTraits(CTFontDescriptorRef descriptor);

// CoreText weight trait -> CSS font-weight, with the same bucket boundaries as
// Chromium's mapping for macOS. A browser that picks a face by CSS weight
// should pick the same face this library reports for that weight, which is
// why the boundaries are copied rather than derived from the nominal weight
// values of the system fonts.
double cssWeight(double coreTextWeight);

// CoreText width trait -> CSS font-stretch percentage (50-200).
double cssWidth(double coreTextWidth);

Style styleOf(const Traits& traits);

// kCTFontPriorityAttribute and kCTFontRegistrationScopeAttribute -> who
// installed the font. Either may be absent.
Scope scopeOf(std::optional<int64_t> priority, std::optional<int64_t> registrationScope);

// kCTFontFormatAttribute -> Format. `hasCff2` tells CFF from CFF2 in an
// OpenType PostScript font, which CoreText reports as one format.
Format formatOf(std::optional<int64_t> coreTextFormat, bool hasCff2);

}  // namespace sfi::platform::darwin
