#include "traits.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace sfi::platform::darwin {
namespace {

struct WeightBucket {
  double low;
  double high;
  double css;
};

// Chromium's buckets for CoreText weights (the first bucket whose range holds
// the value wins, so a value on a boundary belongs to the lighter bucket).
constexpr WeightBucket kWeightBuckets[] = {
    {-1.0, -0.70, 100}, {-0.70, -0.45, 200}, {-0.45, -0.10, 300}, {-0.10, 0.10, 400}, {0.10, 0.27, 500},
    {0.27, 0.35, 600},  {0.35, 0.50, 700},   {0.50, 0.60, 800},   {0.60, 1.0, 900},
};

}  // namespace

Traits readTraits(CTFontDescriptorRef descriptor) {
  Traits traits;
  const auto dictionary = copyAttribute(descriptor, kCTFontTraitsAttribute);
  CFDictionaryRef values = asDictionary(dictionary.get());
  traits.weight = numberValue(dictionaryValue(values, kCTFontWeightTrait)).value_or(0);
  traits.width = numberValue(dictionaryValue(values, kCTFontWidthTrait)).value_or(0);
  traits.slant = numberValue(dictionaryValue(values, kCTFontSlantTrait)).value_or(0);
  traits.symbolic = static_cast<uint32_t>(integerValue(dictionaryValue(values, kCTFontSymbolicTrait)).value_or(0));
  return traits;
}

double cssWeight(double coreTextWeight) {
  const auto bucket = std::find_if(std::begin(kWeightBuckets), std::end(kWeightBuckets), [&](const WeightBucket& b) {
    return b.low <= coreTextWeight && coreTextWeight <= b.high;
  });
  return bucket == std::end(kWeightBuckets) ? 400 : bucket->css;
}

double cssWidth(double coreTextWidth) {
  const double percentage = 100 + coreTextWidth * (coreTextWidth < 0 ? 50 : 100);
  // CoreText stores the trait as a 32-bit float, so 90% arrives as 89.99999985.
  const double rounded = std::round(percentage * 1000) / 1000;
  return std::clamp(rounded, 50.0, 200.0);
}

Style styleOf(const Traits& traits) {
  return (traits.symbolic & kCTFontTraitItalic) ? Style::Italic : Style::Normal;
}

Scope scopeOf(std::optional<int64_t> priority, std::optional<int64_t> registrationScope) {
  // Priority 60000 does not say who registered the font: a process-registered
  // font has it, and so do the system font assets the OS downloads (under
  // /System/Library/AssetsV2), which are registered persistently.
  if (registrationScope == kCTFontManagerScopeProcess) return Scope::Process;
  switch (priority.value_or(0)) {
    case kCTFontPriorityUser:
      return Scope::User;
    case kCTFontPrioritySystem:
    case kCTFontPriorityNetwork:
    case kCTFontPriorityComputer:
      return Scope::System;
    case kCTFontPriorityProcess:
      return registrationScope == kCTFontManagerScopePersistent ? Scope::System : Scope::Unknown;
    default:
      return Scope::Unknown;
  }
}

Format formatOf(std::optional<int64_t> coreTextFormat, bool hasCff2) {
  switch (coreTextFormat.value_or(kCTFontFormatUnrecognized)) {
    case kCTFontFormatOpenTypePostScript:
      return hasCff2 ? Format::Cff2 : Format::Cff;
    case kCTFontFormatOpenTypeTrueType:
    case kCTFontFormatTrueType:
      return Format::TrueType;
    case kCTFontFormatPostScript:
      return Format::Type1;
    case kCTFontFormatBitmap:
      return Format::Bitmap;
    default:
      return Format::Unknown;
  }
}

}  // namespace sfi::platform::darwin
