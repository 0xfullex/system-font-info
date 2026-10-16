#include "platform_value.h"

#include <string>

#include "../../sfnt/sfnt.h"

namespace sfi::platform::darwin {
namespace {

std::string stringAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  return toUtf8(asString(copyAttribute(descriptor, attribute).get()));
}

double numberAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  return numberValue(copyAttribute(descriptor, attribute).get()).value_or(0);
}

bool booleanAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  return booleanValue(copyAttribute(descriptor, attribute).get()).value_or(false);
}

std::string localizedAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  CFStringRef language = nullptr;
  const Ref<CFTypeRef> value(CTFontDescriptorCopyLocalizedAttribute(descriptor, attribute, &language));
  const Ref<CFStringRef> languageOwner(language);
  return toUtf8(asString(value.get()));
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
  out.set("weight", input.traits.weight);
  out.set("width", input.traits.width);
  out.set("slant", input.traits.slant);
  out.set("symbolicTraits", input.traits.symbolic);
  out.set("format", numberAttribute(descriptor, kCTFontFormatAttribute));
  out.set("priority", numberAttribute(descriptor, kCTFontPriorityAttribute));
  out.set("registrationScope", numberAttribute(descriptor, kCTFontRegistrationScopeAttribute));
  out.set("downloadable", booleanAttribute(descriptor, kCTFontDownloadableAttribute));
  out.set("downloaded", booleanAttribute(descriptor, kCTFontDownloadedAttribute));
  out.set("styleName", stringAttribute(descriptor, kCTFontStyleNameAttribute));
  out.set("displayName", stringAttribute(descriptor, kCTFontDisplayNameAttribute));
  out.set("localizedFamilyName", localizedAttribute(descriptor, kCTFontFamilyNameAttribute));
  out.set("localizedStyleName", localizedAttribute(descriptor, kCTFontStyleNameAttribute));
  if (const auto advance = numberValue(copyAttribute(descriptor, kCTFontFixedAdvanceAttribute).get())) out.set("fixedAdvance", *advance);
  if (input.features) out.set("features", *input.features);
  if (input.withMetrics && input.font) out.set("metrics", metricsValue(input.font));
  if (!input.shadowed.empty()) out.set("shadowed", shadowedValue(input.shadowed));
  return out;
}

}  // namespace sfi::platform::darwin
