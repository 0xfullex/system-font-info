#include "variations.h"

#include <cmath>
#include <map>

namespace sfi::platform::darwin {
namespace {

// Coordinates are 16.16 fixed-point numbers in the font, and CoreText reports
// them cut to four decimals (0.62 arrives as 0.6199), so equal values differ by
// up to about 1e-4. The tolerance leaves room above that and is far below the
// distance between two distinct instances.
constexpr double kCoordinateTolerance = 5e-4;

}  // namespace

bool hasVariationAxes(CTFontDescriptorRef descriptor) {
  CFArrayRef axes = asArray(copyAttribute(descriptor, kCTFontVariationAxesAttribute).get());
  return axes && CFArrayGetCount(axes) > 0;
}

SharedAxes makeAxes(CFArrayRef coreTextAxes, const sfnt::FontTables& tables) {
  if (!coreTextAxes || CFArrayGetCount(coreTextAxes) == 0) return nullptr;

  auto axes = std::make_shared<std::vector<VariationAxis>>();
  for (CFIndex i = 0; i < CFArrayGetCount(coreTextAxes); ++i) {
    CFDictionaryRef dictionary = asDictionary(CFArrayGetValueAtIndex(coreTextAxes, i));
    if (!dictionary) continue;
    VariationAxis axis;
    axis.tag = sfnt::tagToString(static_cast<uint32_t>(
        integerValue(dictionaryValue(dictionary, kCTFontVariationAxisIdentifierKey)).value_or(0)));
    axis.min = numberValue(dictionaryValue(dictionary, kCTFontVariationAxisMinimumValueKey)).value_or(0);
    axis.max = numberValue(dictionaryValue(dictionary, kCTFontVariationAxisMaximumValueKey)).value_or(0);
    axis.defaultValue = numberValue(dictionaryValue(dictionary, kCTFontVariationAxisDefaultValueKey)).value_or(0);
    // CoreText's dictionary does not carry the hidden flag in practice (it
    // lists the raw flags under an undocumented key), so it is taken from the
    // `fvar` axis when the dictionary lacks it.
    std::optional<bool> hidden = booleanValue(dictionaryValue(dictionary, kCTFontVariationAxisHiddenKey));
    if (tables.fvar) {
      for (const sfnt::FvarAxis& fvarAxis : tables.fvar->axes) {
        if (sfnt::tagToString(fvarAxis.tag) == axis.tag) {
          axis.names = sfnt::localizedName(tables.names, fvarAxis.nameId);
          if (!hidden) hidden = (fvarAxis.flags & 1) != 0;
          break;
        }
      }
    }
    axis.hidden = hidden.value_or(false);
    axes->push_back(std::move(axis));
  }
  return axes;
}

Coordinates coordinatesOf(CFDictionaryRef variation, const std::vector<VariationAxis>& axes) {
  std::map<std::string, double> explicitValues;
  if (variation) {
    CFDictionaryApplyFunction(
        variation,
        [](const void* key, const void* value, void* context) {
          const auto id = integerValue(static_cast<CFTypeRef>(key));
          const auto number = numberValue(static_cast<CFTypeRef>(value));
          if (id && number)
            (*static_cast<std::map<std::string, double>*>(context))[sfnt::tagToString(static_cast<uint32_t>(*id))] =
                *number;
        },
        &explicitValues);
  }

  Coordinates coordinates;
  for (const VariationAxis& axis : axes) {
    const auto found = explicitValues.find(axis.tag);
    coordinates.emplace_back(axis.tag, found != explicitValues.end() ? found->second : axis.defaultValue);
  }
  return coordinates;
}

std::optional<uint32_t> namedInstanceOf(const Coordinates& coordinates, const std::optional<sfnt::Fvar>& fvar) {
  if (!fvar) return std::nullopt;
  for (size_t i = 0; i < fvar->instances.size(); ++i) {
    const sfnt::FvarInstance& instance = fvar->instances[i];
    if (instance.coordinates.size() != fvar->axes.size()) continue;
    bool matches = true;
    for (size_t axisIndex = 0; axisIndex < fvar->axes.size() && matches; ++axisIndex) {
      const std::string tag = sfnt::tagToString(fvar->axes[axisIndex].tag);
      bool found = false;
      for (const auto& [coordinateTag, value] : coordinates) {
        if (coordinateTag != tag) continue;
        found = true;
        matches = std::fabs(value - instance.coordinates[axisIndex]) <= kCoordinateTolerance;
        break;
      }
      matches = matches && found;
    }
    if (matches) return static_cast<uint32_t>(i);
  }
  return std::nullopt;
}

}  // namespace sfi::platform::darwin
