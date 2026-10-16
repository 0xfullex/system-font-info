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

[[noreturn]] void failFaceIndex(const std::string& path, const std::string& postscriptName, const char* reason) {
  throw std::runtime_error("system-font-info: cannot tell which face of " + path + " is " + postscriptName + ": " +
                           reason);
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
// the face is the one whose `name` table equals the font's.
uint32_t FaceReader::faceIndexInFile(const std::string& path, const std::vector<uint32_t>& tags, CTFontRef font,
                                     const std::string& postscriptName) {
  const FileFaces& faces = fileFaces(path);
  if (!faces.file) {
    if (tags.empty()) return 0;  // not an sfnt, so not a collection
    failFaceIndex(path, postscriptName, "the file cannot be read as an sfnt");
  }
  if (faces.file->faceCount() <= 1) return 0;

  const auto name = copyTable(font, sfnt::makeTag("name"));
  if (!name) failFaceIndex(path, postscriptName, "the font has no name table");
  std::vector<uint32_t> candidates;
  for (size_t i = 0; i < faces.nameTables.size(); ++i) {
    if (faces.nameTables[i] == *name) candidates.push_back(static_cast<uint32_t>(i));
  }
  if (candidates.size() != 1) {
    failFaceIndex(path, postscriptName, candidates.empty() ? "no face has the same name table" : "several faces have the same name table");
  }
  return candidates.front();
}

std::shared_ptr<const FaceReader::Resource> FaceReader::loadResource(CTFontRef font, CTFontDescriptorRef descriptor,
                                                                     const std::vector<uint32_t>& tags,
                                                                     const std::optional<FileLocation>& file) {
  const auto key = file ? std::make_optional(std::make_pair(file->path, file->index)) : std::nullopt;
  if (key) {
    const auto cached = resources_.find(*key);
    if (cached != resources_.end()) return cached->second;
  }

  auto resource = std::make_shared<Resource>();
  resource->tables = sfnt::readFontTables(
      tags, [font](uint32_t tag) { return copyTable(font, tag); }, decodeLegacyName);
  const auto axes = copyAttribute(descriptor, kCTFontVariationAxesAttribute);
  resource->axes = makeAxes(asArray(axes.get()), resource->tables);
  if (key) resources_.emplace(*key, resource);
  return resource;
}

Face FaceReader::read(CTFontDescriptorRef descriptor, const std::vector<ShadowedFile>& shadowed,
                      const AppKitMember* appKitMember) {
  Face face;
  face.postscriptName = stringAttribute(descriptor, kCTFontNameAttribute);

  const Ref<CTFontRef> font(CTFontCreateWithFontDescriptor(descriptor, 0, nullptr));
  if (!font) throw std::runtime_error("system-font-info: CoreText cannot create the font " + face.postscriptName);

  // ---- file, tables
  const std::vector<uint32_t> tags = availableTags(font.get());
  const std::string path = filePath(copyAttribute(descriptor, kCTFontURLAttribute).get());
  if (!path.empty()) face.file = FileLocation{path, faceIndexInFile(path, tags, font.get(), face.postscriptName)};
  const auto resource = loadResource(font.get(), descriptor, tags, face.file);
  const sfnt::FontTables& tables = resource->tables;
  if (options_.tables && !tags.empty()) face.opentype = tables.opentype;

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
    face.coordinates = coordinatesOf(font.get(), *resource->axes);
    face.namedInstance = namedInstanceOf(face.coordinates, tables.fvar);
  }

  // ---- names
  face.familyNames = sfnt::localizedNameWithFallback(tables.names, 16, 1);
  face.styleNames = sfnt::localizedNameWithFallback(tables.names, 17, 2);
  face.fullNames = sfnt::localizedName(tables.names, 4);
  if (face.namedInstance) {
    face.styleNames = sfnt::localizedName(tables.names, tables.fvar->instances[*face.namedInstance].subfamilyNameId);
    face.fullNames = coreTextFullName(font.get());
  }

  if (options_.coverage) face.coverage = coverageRanges(descriptor);

  face.platform = makePlatformValue({descriptor, font.get(), traits, appKitMember, options_.tables, shadowed});
  return face;
}

}  // namespace sfi::platform::darwin
