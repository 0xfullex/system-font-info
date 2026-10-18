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

}  // namespace

FaceReader::FileFaces& FaceReader::fileFaces(const std::string& path) {
  auto [entry, inserted] = files_.try_emplace(path);
  if (inserted) {
    entry->second.file = sfnt::FontFile::open(path);
    if (entry->second.file && entry->second.file->faceCount() > 1) {
      for (size_t i = 0; i < entry->second.file->faceCount(); ++i) {
        entry->second.nameTables.push_back(
            entry->second.file->table(i, sfnt::makeTag("name")).value_or(std::vector<uint8_t>{}));
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
  if (!faces.file)
    return tags.empty() ? std::make_optional(0u) : std::nullopt;  // no tables: not an sfnt, so not a collection
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
  // Finding the index reads the file's collection; only `faces.file` reports it.
  if (!path.empty() && request_.wants(Field::FacesFile)) source.index = faceIndexInFile(path, source.tags, font);
  return source;
}

// The tables of the face of `path` that has `postscriptName`, as its own name
// or as the name of one of its named instances. No source tables and no index
// when the file has no such face.
FaceReader::TableSource FaceReader::tablesFromFile(const std::string& path, const std::string& postscriptName) {
  TableSource source;
  const std::optional<sfnt::FontFile>& openedFile = fileFaces(path).file;
  if (!openedFile) return source;
  const sfnt::FontFile* file = &*openedFile;
  if (const auto faceIndex = file->faceWithPostscriptName(postscriptName, decodeLegacyText)) {
    source.index = static_cast<uint32_t>(*faceIndex);
    source.tags = file->tableTags(*faceIndex);
    source.fetch = [file, face = *faceIndex](uint32_t tag) { return file->table(face, tag); };
  }
  return source;
}

std::shared_ptr<const FaceReader::Resource> FaceReader::loadResource(const TableSource& source,
                                                                     CTFontDescriptorRef descriptor,
                                                                     const std::string& path,
                                                                     std::optional<uint32_t> index) {
  // Faces of a file are told apart by their index; without one nothing is shared.
  const auto key = !path.empty() && index ? std::make_optional(std::make_pair(path, *index)) : std::nullopt;
  if (key) {
    const auto cached = resources_.find(*key);
    if (cached != resources_.end()) return cached->second;
  }

  auto resource = std::make_shared<Resource>();
  resource->tables = sfnt::readFontTables(source.tags, source.fetch, decodeLegacyText);
  if (request_.wantsAnyOpenType()) {
    resource->opentype = sfnt::openTypeValue(resource->tables, request_.openTypeSelection, source.fetch);
    work_.tableEntries += static_cast<uint32_t>(request_.openTypeSelection.entries.count());
  }
  if (request_.wants(Field::FacesAxes) || wantsInstance_) {
    const auto axes = copyAttribute(descriptor, kCTFontVariationAxesAttribute);
    resource->axes = makeAxes(asArray(axes.get()), resource->tables);
  }
  if (key) resources_.emplace(*key, resource);
  return resource;
}

std::optional<Value> FaceReader::featuresOf(const OpenedFont& opened) {
  if (opened.path.empty() || !opened.index) return readFeatures(opened.font.get());
  const auto key = std::make_pair(opened.path, *opened.index);
  const auto cached = features_.find(key);
  if (cached != features_.end()) return cached->second;
  return features_.emplace(key, readFeatures(opened.font.get())).first->second;
}

FaceReader::FaceReader(const Request& request, WorkLog& work) : request_(request), work_(work) {
  for (const Field field : {Field::FacesWeight, Field::FacesWidth, Field::FacesStyle, Field::FacesMonospace,
                            Field::FacesColor, Field::FacesPlatformWeight, Field::FacesPlatformWidth,
                            Field::FacesPlatformSlant, Field::FacesPlatformSymbolicTraits}) {
    wantsTraits_ = wantsTraits_ || request.wants(field);
  }
  for (const Field field :
       {Field::FacesCoordinates, Field::FacesNamedInstanceIndex, Field::FacesFullNames, Field::FacesStyleNames}) {
    wantsInstance_ = wantsInstance_ || request.wants(field);
  }
}

FaceReader::OpenedFont FaceReader::openFont(CTFontDescriptorRef descriptor, const std::string& postscriptName) {
  OpenedFont opened;
  opened.font = Ref<CTFontRef>(CTFontCreateWithFontDescriptor(descriptor, 0, nullptr));
  if (!opened.font) throw std::runtime_error("system-font-info: CoreText cannot create the font " + postscriptName);
  ++work_.fontsOpened;

  // CoreText can create another font than the descriptor names (it substitutes
  // one when it cannot use the file). Whatever is read from that CTFont then
  // describes the wrong font, so the tables come from the descriptor's file and
  // the data only a CTFont provides is left out.
  const Ref<CFStringRef> createdName(CTFontCopyPostScriptName(opened.font.get()));
  opened.substituted = !postscriptName.empty() && toUtf8(createdName.get()) != postscriptName;

  opened.path = filePath(copyAttribute(descriptor, kCTFontURLAttribute).get());
  const TableSource source =
      opened.substituted ? tablesFromFile(opened.path, postscriptName) : tablesFromFont(opened.font.get(), opened.path);
  opened.index = source.index;
  opened.isSfnt = !source.tags.empty();
  opened.resource = loadResource(source, descriptor, opened.path, opened.index);
  return opened;
}

Face FaceReader::read(CTFontDescriptorRef descriptor, const std::string& postscriptName,
                      const std::vector<ShadowedFile>& shadowed) {
  Face face;
  face.postscriptName = postscriptName;

  // Group C is done for a face at most once, by the first field that needs it. A face whose fields are all descriptor
  // attributes never creates a font.
  std::optional<OpenedFont> openedFont;
  const auto font = [&]() -> const OpenedFont& {
    if (!openedFont) openedFont = openFont(descriptor, postscriptName);
    return *openedFont;
  };

  // ---- descriptor attributes (B)
  Traits traits;
  if (wantsTraits_) traits = readTraits(descriptor);  // without it the conversions below give defaults nobody reads
  face.weight = cssWeight(traits.weight);
  face.width = cssWidth(traits.width);
  face.style = styleOf(traits);
  face.monospace = traits.symbolic.value_or(0) & kCTFontTraitMonoSpace;
  face.color = traits.symbolic.value_or(0) & kCTFontTraitColorGlyphs;
  if (request_.wants(Field::FacesEnabled)) {
    // CoreText stores the enabled state as a number or a boolean; a font without one is enabled.
    face.enabled = booleanValue(copyAttribute(descriptor, kCTFontEnabledAttribute).get()).value_or(true);
  }
  if (request_.wants(Field::FacesSupportedLanguages)) {
    if (const auto languages = copyAttribute(descriptor, kCTFontLanguagesAttribute); asArray(languages.get())) {
      for (CFIndex i = 0; i < CFArrayGetCount(asArray(languages.get())); ++i) {
        face.supportedLanguages.push_back(toUtf8(asString(CFArrayGetValueAtIndex(asArray(languages.get()), i))));
      }
    }
  }
  if (request_.wants(Field::FacesScope)) {
    face.scope = scopeOf(integerValue(copyAttribute(descriptor, kCTFontPriorityAttribute).get()),
                         integerValue(copyAttribute(descriptor, kCTFontRegistrationScopeAttribute).get()));
  }
  if (request_.wants(Field::FacesVariable)) face.variable = hasVariationAxes(descriptor);

  // ---- file, tables (C)
  if (request_.wants(Field::FacesFile)) {
    const OpenedFont& opened = font();
    if (!opened.path.empty()) face.file = FileLocation{opened.path, opened.index};
  }
  if (request_.wants(Field::FacesSymbol)) face.symbol = font().resource->tables.symbolCmap;
  if (request_.wants(Field::FacesFormat)) {
    face.format = formatOf(integerValue(copyAttribute(descriptor, kCTFontFormatAttribute).get()),
                           [&] { return font().resource->tables.hasCff2; });
  }
  if (request_.wantsAnyOpenType()) {
    const OpenedFont& opened = font();
    if (opened.isSfnt) face.opentype = opened.resource->opentype;
  }

  // ---- variations (C)
  if (request_.wants(Field::FacesAxes) || wantsInstance_) {
    const OpenedFont& opened = font();
    if (opened.resource->axes) {
      face.axes = opened.resource->axes;
      if (wantsInstance_) {
        const Ref<CFTypeRef> variation = opened.substituted ? copyAttribute(descriptor, kCTFontVariationAttribute)
                                                            : Ref<CFTypeRef>(CTFontCopyVariation(opened.font.get()));
        face.coordinates = coordinatesOf(asDictionary(variation.get()), *opened.resource->axes);
        face.namedInstanceIndex = namedInstanceOf(face.coordinates, opened.resource->tables.fvar);
      }
    }
  }

  // ---- names (C)
  if (request_.wants(Field::FacesFamilyNames)) {
    face.familyNames = sfnt::localizedNameWithFallback(font().resource->tables.names, 16, 1);
  }
  if (request_.wants(Field::FacesStyleNames) || request_.wants(Field::FacesFullNames)) {
    const sfnt::FontTables& tables = font().resource->tables;
    if (face.namedInstanceIndex) {
      const sfnt::FvarInstance& instance = tables.fvar->instances[*face.namedInstanceIndex];
      face.styleNames = sfnt::localizedName(tables.names, instance.subfamilyNameId);
      // A named instance has no full-name record of its own; its names are composed from the family and its subfamily
      // name.
      face.fullNames = sfnt::instanceFullNames(tables.names, instance);
    } else {
      face.styleNames = sfnt::localizedNameWithFallback(tables.names, 17, 2);
      face.fullNames = sfnt::localizedName(tables.names, 4);
    }
  }

  // ---- character set (E)
  if (request_.wants(Field::FacesCoverage)) {
    face.coverage = coverageRanges(descriptor);
    ++work_.characterSets;
  }

  // ---- platform
  if (request_.platformObject) {
    // Only a CTFont provides the features and the metrics.
    std::optional<Value> features;
    CTFontRef metricsFont = nullptr;
    if (request_.wants(Field::FacesPlatformFeatures) && !font().substituted) features = featuresOf(font());
    if (request_.wants(Field::FacesPlatformMetrics) && !font().substituted) metricsFont = font().font.get();
    face.platform =
        makePlatformValue({request_, descriptor, metricsFont, features ? &*features : nullptr, traits, shadowed});
  }
  return face;
}

}  // namespace sfi::platform::darwin
