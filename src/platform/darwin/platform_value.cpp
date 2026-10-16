#include "platform_value.h"

#include <optional>
#include <string>

#include "../../sfnt/sfnt.h"

namespace sfi::platform::darwin {
namespace {

std::optional<std::string> stringAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  const auto value = copyAttribute(descriptor, attribute);
  if (CFStringRef text = asString(value.get())) return toUtf8(text);
  return std::nullopt;
}

std::optional<std::string> localizedAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  CFStringRef language = nullptr;
  const Ref<CFTypeRef> value(CTFontDescriptorCopyLocalizedAttribute(descriptor, attribute, &language));
  const Ref<CFStringRef> languageOwner(language);
  if (CFStringRef text = asString(value.get())) return toUtf8(text);
  return std::nullopt;
}

// Sets `key` when the value is there: a value CoreText does not give is left
// out, not replaced by a default.
template <typename T>
void setIfPresent(Value& object, const char* key, const std::optional<T>& value) {
  if (value) object.set(key, *value);
}

// An OpenType tag CoreText reports as text ("liga") or as a four-character code.
std::optional<std::string> openTypeTag(CFDictionaryRef dictionary) {
  CFTypeRef tag = dictionaryValue(dictionary, kCTFontOpenTypeFeatureTag);
  if (CFStringRef text = asString(tag)) return toUtf8(text);
  if (const auto code = integerValue(tag)) return sfnt::tagToString(static_cast<uint32_t>(*code));
  return std::nullopt;
}

Value selectorValue(CFDictionaryRef selector) {
  Value out = Value::object();
  out.set("id", integerValue(dictionaryValue(selector, kCTFontFeatureSelectorIdentifierKey)).value_or(0));
  out.set("name", toUtf8(asString(dictionaryValue(selector, kCTFontFeatureSelectorNameKey))));
  out.set("default", booleanValue(dictionaryValue(selector, kCTFontFeatureSelectorDefaultKey)).value_or(false));
  if (const auto tag = openTypeTag(selector)) out.set("openTypeTag", *tag);
  if (const auto value = integerValue(dictionaryValue(selector, kCTFontOpenTypeFeatureValue))) out.set("openTypeValue", *value);
  return out;
}

Value featureValue(CFDictionaryRef feature) {
  Value out = Value::object();
  out.set("id", integerValue(dictionaryValue(feature, kCTFontFeatureTypeIdentifierKey)).value_or(0));
  out.set("name", toUtf8(asString(dictionaryValue(feature, kCTFontFeatureTypeNameKey))));
  out.set("exclusive", booleanValue(dictionaryValue(feature, kCTFontFeatureTypeExclusiveKey)).value_or(false));
  if (const auto tag = openTypeTag(feature)) out.set("openTypeTag", *tag);
  Value selectors = Value::array();
  if (CFArrayRef list = asArray(dictionaryValue(feature, kCTFontFeatureTypeSelectorsKey))) {
    for (CFIndex i = 0; i < CFArrayGetCount(list); ++i) {
      if (CFDictionaryRef selector = asDictionary(CFArrayGetValueAtIndex(list, i))) selectors.push(selectorValue(selector));
    }
  }
  out.set("selectors", std::move(selectors));
  return out;
}

// CTFont reports lengths in points at the size the font was created with;
// font units are what the font file stores.
Value metricsValue(CTFontRef font) {
  const double unitsPerEm = CTFontGetUnitsPerEm(font);
  const double toFontUnits = unitsPerEm / CTFontGetSize(font);
  const CGRect box = CTFontGetBoundingBox(font);

  Value out = Value::object();
  out.set("ascent", CTFontGetAscent(font) * toFontUnits);
  out.set("descent", CTFontGetDescent(font) * toFontUnits);
  out.set("leading", CTFontGetLeading(font) * toFontUnits);
  out.set("capHeight", CTFontGetCapHeight(font) * toFontUnits);
  out.set("xHeight", CTFontGetXHeight(font) * toFontUnits);
  out.set("slantAngle", CTFontGetSlantAngle(font));  // degrees, not a length
  out.set("underlinePosition", CTFontGetUnderlinePosition(font) * toFontUnits);
  out.set("underlineThickness", CTFontGetUnderlineThickness(font) * toFontUnits);
  out.set("unitsPerEm", unitsPerEm);
  out.set("glyphCount", static_cast<double>(CTFontGetGlyphCount(font)));
  Value boundingBox = Value::array();
  for (const CGFloat component : {box.origin.x, box.origin.y, box.size.width, box.size.height}) boundingBox.push(component * toFontUnits);
  out.set("boundingBox", std::move(boundingBox));
  return out;
}

Value shadowedValue(const std::vector<ShadowedFile>& shadowed) {
  Value out = Value::array();
  for (const ShadowedFile& file : shadowed) {
    Value entry = Value::object();
    entry.set("path", file.path);
    entry.set("variable", file.variable);
    out.push(std::move(entry));
  }
  return out;
}

}  // namespace

// The features of the font with their names. CTFontCopyFeatures is used and
// not kCTFontFeaturesAttribute because the attribute lists only identifiers;
// the names come from the font itself, in the language of the process.
std::optional<Value> readFeatures(CTFontRef font) {
  const Ref<CFArrayRef> features(CTFontCopyFeatures(font));
  if (!features) return std::nullopt;
  Value out = Value::array();
  for (CFIndex i = 0; i < CFArrayGetCount(features.get()); ++i) {
    if (CFDictionaryRef feature = asDictionary(CFArrayGetValueAtIndex(features.get(), i))) out.push(featureValue(feature));
  }
  return out;
}

Value makePlatformValue(const PlatformValueInput& input) {
  CTFontDescriptorRef descriptor = input.descriptor;

  Value out = Value::object();
  out.set("os", "darwin");
  setIfPresent(out, "weight", input.traits.weight);
  setIfPresent(out, "width", input.traits.width);
  setIfPresent(out, "slant", input.traits.slant);
  setIfPresent(out, "symbolicTraits", input.traits.symbolic);
  setIfPresent(out, "format", numberValue(copyAttribute(descriptor, kCTFontFormatAttribute).get()));
  setIfPresent(out, "priority", numberValue(copyAttribute(descriptor, kCTFontPriorityAttribute).get()));
  setIfPresent(out, "registrationScope", numberValue(copyAttribute(descriptor, kCTFontRegistrationScopeAttribute).get()));
  setIfPresent(out, "downloadable", booleanValue(copyAttribute(descriptor, kCTFontDownloadableAttribute).get()));
  setIfPresent(out, "downloaded", booleanValue(copyAttribute(descriptor, kCTFontDownloadedAttribute).get()));
  setIfPresent(out, "styleName", stringAttribute(descriptor, kCTFontStyleNameAttribute));
  setIfPresent(out, "displayName", stringAttribute(descriptor, kCTFontDisplayNameAttribute));
  setIfPresent(out, "localizedFamilyName", localizedAttribute(descriptor, kCTFontFamilyNameAttribute));
  setIfPresent(out, "localizedStyleName", localizedAttribute(descriptor, kCTFontStyleNameAttribute));
  setIfPresent(out, "fixedAdvance", numberValue(copyAttribute(descriptor, kCTFontFixedAdvanceAttribute).get()));
  if (input.features) out.set("features", *input.features);
  if (input.withMetrics && input.font) out.set("metrics", metricsValue(input.font));
  if (!input.shadowed.empty()) out.set("shadowed", shadowedValue(input.shadowed));
  return out;
}

}  // namespace sfi::platform::darwin
