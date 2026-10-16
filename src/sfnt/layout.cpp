// GSUB and GPOS share one layout skeleton (ScriptList, FeatureList); only the
// lookups differ, and those are not read. The result lists which features each
// script and language system enables, plus the UI names of stylistic sets and
// character variants.
#include <algorithm>

#include "internal.h"
#include "reader.h"

namespace sfi::sfnt {

namespace {

constexpr uint16_t kNoRequiredFeature = 0xFFFF;
constexpr size_t kTagRecordSize = 6;  // tag + Offset16, in the script, language system and feature lists

struct Feature {
  std::string tag;
  size_t offset = 0;  // of the Feature table, from the start of the layout table
};

void addUnique(std::vector<std::string>& list, const std::string& tag) {
  if (std::find(list.begin(), list.end(), tag) == list.end()) list.push_back(tag);
}

bool isNumberedTag(const std::string& tag, char first, char second) {
  return tag.size() == 4 && tag[0] == first && tag[1] == second && tag[2] >= '0' && tag[2] <= '9' &&
         tag[3] >= '0' && tag[3] <= '9';
}

// The feature tags a language system enables: the required feature first, then
// the listed ones in order, each tag once.
Value languageSystemFeatures(const Reader& r, size_t langSys, const std::vector<Feature>& features) {
  const uint16_t required = r.u16(langSys + 2);
  const size_t count = r.u16(langSys + 4);
  r.require(langSys + 6, count * 2);
  std::vector<std::string> tags;
  auto add = [&](size_t index) {
    if (index >= features.size()) throw ParseError("feature index out of range");
    addUnique(tags, features[index].tag);
  };
  if (required != kNoRequiredFeature) add(required);
  for (size_t i = 0; i < count; ++i) add(r.u16(langSys + 6 + i * 2));

  Value out = Value::array();
  for (const std::string& tag : tags) out.push(tag);
  return out;
}

}  // namespace

Value layoutToValue(std::span<const uint8_t> table, const std::vector<NameRecord>& names) {
  const Reader r(table);
  if (r.u16(0) != 1) throw ParseError("unknown layout table version");
  const size_t scriptListOffset = r.u16(4);
  const size_t featureListOffset = r.u16(6);

  std::vector<Feature> features;
  if (featureListOffset != 0) {
    const size_t count = r.u16(featureListOffset);
    r.require(featureListOffset + 2, count * kTagRecordSize);
    for (size_t i = 0; i < count; ++i) {
      const size_t at = featureListOffset + 2 + i * kTagRecordSize;
      features.push_back({tagToString(r.u32(at)), featureListOffset + r.u16(at + 4)});
    }
  }

  Value scripts = Value::object();
  if (scriptListOffset != 0) {
    const size_t scriptCount = r.u16(scriptListOffset);
    r.require(scriptListOffset + 2, scriptCount * kTagRecordSize);
    for (size_t i = 0; i < scriptCount; ++i) {
      const size_t at = scriptListOffset + 2 + i * kTagRecordSize;
      const size_t script = scriptListOffset + r.u16(at + 4);
      const size_t defaultLangSys = r.u16(script);
      const size_t langSysCount = r.u16(script + 2);
      r.require(script + 4, langSysCount * kTagRecordSize);

      Value languages = Value::object();
      if (defaultLangSys != 0) languages.set("dflt", languageSystemFeatures(r, script + defaultLangSys, features));
      for (size_t j = 0; j < langSysCount; ++j) {
        const size_t langAt = script + 4 + j * kTagRecordSize;
        languages.set(tagToString(r.u32(langAt)),
                      languageSystemFeatures(r, script + r.u16(langAt + 4), features));
      }
      scripts.set(tagToString(r.u32(at)), std::move(languages));
    }
  }

  // ssXX carries FeatureParamsStylisticSet and cvXX FeatureParamsCharacterVariants;
  // both start with a version and then the nameID of the UI label (0 = none).
  // Several features may share a tag, so the names of all of them are merged.
  std::vector<std::string> distinctTags;
  std::vector<std::pair<std::string, LocalizedStrings>> featureNames;
  for (const Feature& feature : features) {
    addUnique(distinctTags, feature.tag);
    if (!isNumberedTag(feature.tag, 's', 's') && !isNumberedTag(feature.tag, 'c', 'v')) continue;
    const size_t paramsOffset = r.u16(feature.offset);
    if (paramsOffset == 0) continue;
    const uint16_t nameId = r.u16(feature.offset + paramsOffset + 2);
    if (nameId == 0) continue;

    auto entry = std::find_if(featureNames.begin(), featureNames.end(), [&](const auto& e) { return e.first == feature.tag; });
    if (entry == featureNames.end()) entry = featureNames.insert(featureNames.end(), {feature.tag, {}});
    for (const LocalizedString& name : localizedName(names, nameId)) appendUniqueString(entry->second, name);
  }

  Value featureTags = Value::array();
  for (const std::string& tag : distinctTags) featureTags.push(tag);
  Value out = Value::object();
  out.set("scripts", std::move(scripts)).set("features", std::move(featureTags));

  Value namesByTag = Value::object();
  bool hasNames = false;
  for (const auto& [tag, strings] : featureNames) {
    if (strings.empty()) continue;
    namesByTag.set(tag, localizedStringsToValue(strings));
    hasNames = true;
  }
  if (hasNames) out.set("featureNames", std::move(namesByTag));
  return out;
}

}  // namespace sfi::sfnt
