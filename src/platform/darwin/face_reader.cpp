#include "face_reader.h"

#include <algorithm>
#include <stdexcept>

#include "../../core/platform.h"
#include "coverage.h"
#include "platform_value.h"
#include "traits.h"
#include "variations.h"

namespace sfi::platform::darwin {
namespace {

std::string stringAttribute(CTFontDescriptorRef descriptor, CFStringRef attribute) {
  return toUtf8(asString(copyAttribute(descriptor, attribute).get()));
}

std::optional<std::vector<uint8_t>> copyTable(CTFontRef font, uint32_t tag) {
  const Ref<CFDataRef> data(CTFontCopyTable(font, tag, kCTFontTableOptionNoOptions));
  if (!data) return std::nullopt;
  const UInt8* bytes = CFDataGetBytePtr(data.get());
  return std::vector<uint8_t>(bytes, bytes + CFDataGetLength(data.get()));
}

// Tags of the tables CTFont can hand out; empty for a font that is not an sfnt.
std::vector<uint32_t> availableTags(CTFontRef font) {
  std::vector<uint32_t> tags;
  const Ref<CFArrayRef> array(CTFontCopyAvailableTables(font, kCTFontTableOptionNoOptions));
  if (!array) return tags;
  // The array holds the tags themselves in its pointer-sized slots, not CFNumbers.
  for (CFIndex i = 0; i < CFArrayGetCount(array.get()); ++i) {
    tags.push_back(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(array.get(), i))));
  }
  return tags;
}

// CoreText names languages with BCP 47 tags, written with an underscore at
// times ("zh_Hans"); the BCP 47 separator is a hyphen.
std::string languageTag(CFStringRef language) {
  std::string tag = toUtf8(language);
  std::replace(tag.begin(), tag.end(), '_', '-');
  return tag;
}

// The face's own full name as CoreText reports it, in the one language it
// picked. Named instances of a variable font have no `name` record of their
// own for it; CoreText composes it from the family and the instance name.
LocalizedStrings coreTextFullName(CTFontRef font) {
  CFStringRef language = nullptr;
  const Ref<CFStringRef> name(CTFontCopyLocalizedName(font, kCTFontFullNameKey, &language));
  const Ref<CFStringRef> languageOwner(language);
  if (!name) return {};
  return {{languageTag(language), toUtf8(name.get())}};
}

}  // namespace

FaceReader::FileFaces& FaceReader::fileFaces(const std::string& path) {
  auto [entry, inserted] = files_.try_emplace(path);
  if (inserted) {
    entry->second.file = sfnt::FontFile::open(path);
    if (entry->second.file && entry->second.file->faceCount() > 1) {
      for (size_t i = 0; i < entry->second.file->faceCount(); ++i) {
        entry->second.nameTables.push_back(entry->second.file->table(i, sfnt::makeTag("name")).value_or(std::vector<uint8_t>{}));
      }
    }
  }
  return entry->second;
}

// CoreText does not say which face of a collection a font is. Two faces of a
// collection are never the same font, and a font's `name` table names it, so
// the face is the one whose `name` table equals the font's. Unset when that
// does not single out a face, or when the file is one the sfnt reader cannot
// open and the font is nevertheless an sfnt (a .dfont suitcase can hold several).
std::optional<uint32_t> FaceReader::faceIndexInFile(const std::string& path, const std::vector<uint32_t>& tags,
                                                    CTFontRef font) {
  const FileFaces& faces = fileFaces(path);
  if (!faces.file) return tags.empty() ? std::make_optional(0u) : std::nullopt;  // no tables: not an sfnt, so not a collection
  if (faces.file->faceCount() <= 1) return 0;

  const auto name = copyTable(font, sfnt::makeTag("name"));
  if (!name) return std::nullopt;
  std::optional<uint32_t> found;
  for (size_t i = 0; i < faces.nameTables.size(); ++i) {
    if (faces.nameTables[i] != *name) continue;
    if (found) return std::nullopt;
    found = static_cast<uint32_t>(i);
  }
  return found;
}

FaceReader::TableSource FaceReader::tablesFromFont(CTFontRef font, const std::string& path) {
  TableSource source;
  source.tags = availableTags(font);
  source.fetch = [font](uint32_t tag) { return copyTable(font, tag); };
  if (!path.empty()) source.index = faceIndexInFile(path, source.tags, font);
  return source;
}

// The tables of the face of `path` whose `name` table gives `postscriptName`.
// No source tables and no index when the file has no such face.
FaceReader::TableSource FaceReader::tablesFromFile(const std::string& path, const std::string& postscriptName) {
  TableSource source;
  const std::optional<sfnt::FontFile>& openedFile = fileFaces(path).file;
  if (!openedFile) return source;
  const sfnt::FontFile* file = &*openedFile;
  for (size_t i = 0; i < file->faceCount() && !source.index; ++i) {
    const auto name = file->table(i, sfnt::makeTag("name"));
    if (!name) continue;
    try {
      for (const sfnt::NameRecord& record : sfnt::parseNameTable(*name, decodeLegacyName)) {
        if (record.nameId == 6 && record.value == postscriptName) source.index = static_cast<uint32_t>(i);
      }
    } catch (const sfnt::ParseError&) {
    }
  }
  if (source.index) {
    const size_t faceIndex = *source.index;
    source.tags = file->tableTags(faceIndex);
    source.fetch = [file, faceIndex](uint32_t tag) { return file->table(faceIndex, tag); };
  }
  return source;
}

std::shared_ptr<const FaceReader::Resource> FaceReader::loadResource(const TableSource& source,
                                                                     CTFontDescriptorRef descriptor,
                                                                     const std::optional<FileLocation>& file) {
  // Faces of a file are told apart by their index; without one nothing is shared.
  const auto key = file && file->index ? std::make_optional(std::make_pair(file->path, *file->index)) : std::nullopt;
  if (key) {
    const auto cached = resources_.find(*key);
    if (cached != resources_.end()) return cached->second;
  }

  auto resource = std::make_shared<Resource>();
  resource->tables = sfnt::readFontTables(source.tags, source.fetch, decodeLegacyName);
  const auto axes = copyAttribute(descriptor, kCTFontVariationAxesAttribute);
  resource->axes = makeAxes(asArray(axes.get()), resource->tables);
  if (key) resources_.emplace(*key, resource);
  return resource;
}

std::optional<Value> FaceReader::featuresOf(CTFontRef font, const std::optional<FileLocation>& file) {
  if (!file || !file->index) return readFeatures(font);
  const auto key = std::make_pair(file->path, *file->index);
  const auto cached = features_.find(key);
  if (cached != features_.end()) return cached->second;
  return features_.emplace(key, readFeatures(font)).first->second;
}

Face FaceReader::read(CTFontDescriptorRef descriptor, const std::vector<ShadowedFile>& shadowed,
                      const AppKitMember* appKitMember) {
  Face face;
  face.postscriptName = stringAttribute(descriptor, kCTFontNameAttribute);

  const Ref<CTFontRef> font(CTFontCreateWithFontDescriptor(descriptor, 0, nullptr));
  if (!font) throw std::runtime_error("system-font-info: CoreText cannot create the font " + face.postscriptName);

  // CoreText can create another font than the descriptor names (it substitutes
  // one when it cannot use the file). Whatever is read from that CTFont then
  // describes the wrong font, so the tables come from the descriptor's file and
  // the data only a CTFont provides is left out.
  const Ref<CFStringRef> createdName(CTFontCopyPostScriptName(font.get()));
  const bool substituted = !face.postscriptName.empty() && toUtf8(createdName.get()) != face.postscriptName;

  // ---- file, tables
  const std::string path = filePath(copyAttribute(descriptor, kCTFontURLAttribute).get());
  const TableSource source = substituted ? tablesFromFile(path, face.postscriptName) : tablesFromFont(font.get(), path);
  if (!path.empty()) face.file = FileLocation{path, source.index};
  const auto resource = loadResource(source, descriptor, face.file);
  const sfnt::FontTables& tables = resource->tables;
  if (options_.tables && !source.tags.empty()) face.opentype = tables.opentype;

  // ---- classification
  const Traits traits = readTraits(descriptor);
  face.weight = cssWeight(traits.weight);
  face.width = cssWidth(traits.width);
  face.style = styleOf(traits);
  face.monospace = traits.symbolic & kCTFontTraitMonoSpace;
  face.color = traits.symbolic & kCTFontTraitColorGlyphs;
  face.symbol = tables.symbolCmap;
  // CoreText stores the enabled state as a number or a boolean; a font without one is enabled.
  face.enabled = booleanValue(copyAttribute(descriptor, kCTFontEnabledAttribute).get()).value_or(true);
  if (const auto languages = copyAttribute(descriptor, kCTFontLanguagesAttribute); asArray(languages.get())) {
    for (CFIndex i = 0; i < CFArrayGetCount(asArray(languages.get())); ++i) {
      face.languages.push_back(toUtf8(asString(CFArrayGetValueAtIndex(asArray(languages.get()), i))));
    }
  }
  face.scope = scopeOf(integerValue(copyAttribute(descriptor, kCTFontPriorityAttribute).get()),
                       integerValue(copyAttribute(descriptor, kCTFontRegistrationScopeAttribute).get()));
  face.format = formatOf(integerValue(copyAttribute(descriptor, kCTFontFormatAttribute).get()), tables.hasCff2);

  // ---- variations
  if (resource->axes) {
    face.variable = true;
    face.axes = resource->axes;
    const Ref<CFTypeRef> variation = substituted ? copyAttribute(descriptor, kCTFontVariationAttribute)
                                                 : Ref<CFTypeRef>(CTFontCopyVariation(font.get()));
    face.coordinates = coordinatesOf(asDictionary(variation.get()), *resource->axes);
    face.namedInstance = namedInstanceOf(face.coordinates, tables.fvar);
  }

  // ---- names
  face.familyNames = sfnt::localizedNameWithFallback(tables.names, 16, 1);
  face.styleNames = sfnt::localizedNameWithFallback(tables.names, 17, 2);
  face.fullNames = sfnt::localizedName(tables.names, 4);
  if (face.namedInstance) {
    face.styleNames = sfnt::localizedName(tables.names, tables.fvar->instances[*face.namedInstance].subfamilyNameId);
    face.fullNames = substituted ? LocalizedStrings{} : coreTextFullName(font.get());
  }

  if (options_.coverage) face.coverage = coverageRanges(descriptor);

  // Only a CTFont provides the features and the metrics.
  const std::optional<Value> features = substituted ? std::nullopt : featuresOf(font.get(), face.file);
  face.platform = makePlatformValue({descriptor, substituted ? nullptr : font.get(), features ? &*features : nullptr, traits,
                                     appKitMember, options_.tables, shadowed});
  return face;
}

}  // namespace sfi::platform::darwin
