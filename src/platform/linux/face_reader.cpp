#include "face_reader.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRUETYPE_TABLES_H

#include <array>
#include <fstream>
#include <memory>
#include <string_view>

#include "traits.h"

namespace sfi::platform::linux_backend {
namespace {
LocalizedStrings names(FcPattern* pattern, const char* values, const char* languages) {
  LocalizedStrings result;
  for (int i = 0;; ++i) {
    auto value = string(pattern, values, i);
    if (!value) return result;
    result.push_back({string(pattern, languages, i).value_or(""), *value});
  }
}

std::vector<std::pair<uint32_t, uint32_t>> ranges(FcCharSet* characters) {
  std::vector<std::pair<uint32_t, uint32_t>> result;
  FcChar32 map[FC_CHARSET_MAP_SIZE];
  FcChar32 next = 0;
  for (FcChar32 page = FcCharSetFirstPage(characters, map, &next); page != FC_CHARSET_DONE;
       page = FcCharSetNextPage(characters, map, &next)) {
    for (uint32_t word = 0; word < FC_CHARSET_MAP_SIZE; ++word) {
      for (uint32_t bit = 0; bit < 32; ++bit) {
        if (!(map[word] & (FcChar32(1) << bit))) continue;
        uint32_t codePoint = page + word * 32 + bit;
        if (!result.empty() && result.back().second + 1 == codePoint)
          result.back().second = codePoint;
        else
          result.emplace_back(codePoint, codePoint);
      }
    }
  }
  return result;
}

Value platformValue(FcPattern* pattern) {
  Value result = Value::object();
  result.set("os", "linux");
  // The fixed scalar object set is cheap; the conversion selects the requested keys.
  const std::pair<const char*, const char*> numbers[] = {
      {"weight", FC_WEIGHT},   {"width", FC_WIDTH}, {"slant", FC_SLANT},         {"index", FC_INDEX},
      {"spacing", FC_SPACING}, {"order", FC_ORDER}, {"pixelSize", FC_PIXEL_SIZE}};
  for (auto [key, object] : numbers)
    if (auto value = number(pattern, object)) result.set(key, *value);
  const std::pair<const char*, const char*> strings[] = {
      {"format", FC_FONTFORMAT}, {"fontWrapper", FC_FONT_WRAPPER}, {"foundry", FC_FOUNDRY}};
  for (auto [key, object] : strings)
    if (auto value = string(pattern, object)) result.set(key, *value);
  const std::pair<const char*, const char*> booleans[] = {{"decorative", FC_DECORATIVE},
                                                          {"fontHasHint", FC_FONT_HAS_HINT},
                                                          {"outline", FC_OUTLINE},
                                                          {"scalable", FC_SCALABLE},
                                                          {"antialias", FC_ANTIALIAS}};
  for (auto [key, object] : booleans)
    if (auto value = boolean(pattern, object)) result.set(key, *value);
  return result;
}

// WOFF and WOFF2 wrap an sfnt that the table parser cannot read. The signature is the file's own statement of what it
// is; fontconfig's FC_FONT_WRAPPER says the same only from version 2.15.
bool isWebFont(const std::string& path) {
  std::array<char, 4> signature{};
  std::ifstream file(path, std::ios::binary);
  if (!file.read(signature.data(), signature.size())) return false;
  const std::string_view tag(signature.data(), signature.size());
  return tag == "wOFF" || tag == "wOF2";
}

struct FreeTypeLibrary {
  FT_Library value = nullptr;
  FreeTypeLibrary() { require(FT_Init_FreeType(&value) == 0, "FT_Init_FreeType"); }
  ~FreeTypeLibrary() { FT_Done_FreeType(value); }
};
struct FreeTypeFace {
  FT_Face value = nullptr;
  ~FreeTypeFace() {
    if (value) FT_Done_Face(value);
  }
};
}  // namespace

// A file that cannot be read gives the resource no tables and no `opentype` value: the fields that need the file stay
// absent and the call goes on, since one unreadable file must not fail the listing of every other font.
const FaceReader::Resource& FaceReader::readResource(const Identity& id) {
  auto [entry, inserted] = resources_.try_emplace(id);
  if (!inserted) return entry->second;
  auto& result = entry->second;
  ++work_.fontsOpened;
  auto file = sfnt::FontFile::open(id.path);
  std::vector<uint32_t> tags;
  sfnt::TableFetcher fetch;
  std::unique_ptr<FreeTypeLibrary> library;
  FreeTypeFace face;
  if (file && id.index < file->faceCount()) {
    tags = file->tableTags(id.index);
    fetch = [&](uint32_t tag) { return file->table(id.index, tag); };
  } else if (isWebFont(id.path)) {
    library = std::make_unique<FreeTypeLibrary>();
    if (FT_New_Face(library->value, id.path.c_str(), id.index, &face.value) != 0) return result;
    FT_ULong count = 0;
    if (FT_Sfnt_Table_Info(face.value, 0, nullptr, &count) != 0) return result;
    for (FT_ULong i = 0; i < count; ++i) {
      FT_ULong tag = 0, length = 0;
      if (FT_Sfnt_Table_Info(face.value, i, &tag, &length) != 0) return result;
      tags.push_back(uint32_t(tag));
    }
    fetch = [&](uint32_t tag) -> std::optional<std::vector<uint8_t>> {
      FT_ULong length = 0;
      if (FT_Load_Sfnt_Table(face.value, tag, 0, nullptr, &length) != 0) return std::nullopt;
      std::vector<uint8_t> bytes(length);
      if (FT_Load_Sfnt_Table(face.value, tag, 0, bytes.data(), &length) != 0) return std::nullopt;
      return bytes;
    };
  } else {
    return result;
  }
  result.tables = sfnt::readFontTables(tags, fetch, decodeLegacyText);
  if (request_.wantsAnyOpenType()) {
    result.opentype = sfnt::openTypeValue(result.tables, request_.openTypeSelection, fetch);
    work_.tableEntries += uint32_t(request_.openTypeSelection.entries.count());
  }
  if (request_.wants(Field::FacesAxes) && result.tables.fvar) {
    auto axes = std::make_shared<std::vector<VariationAxis>>();
    for (const auto& axis : result.tables.fvar->axes) {
      axes->push_back({sfnt::tagToString(axis.tag), sfnt::localizedName(result.tables.names, axis.nameId), axis.min,
                       axis.defaultValue, axis.max, bool(axis.flags & 1)});
    }
    result.axes = std::move(axes);
  }
  return result;
}

FaceReader::FaceReader(const Request& request, WorkLog& work, const std::set<Identity>& variableResources,
                       const std::vector<FcPattern*>& members)
    : request_(request), work_(work), variableResources_(variableResources) {
  for (auto member : members) {
    const Identity id = identity(member);
    if (!variableResources_.contains(resource(id))) continue;
    auto [kept, added] = keptWithoutFvar_.try_emplace(resource(id), member);
    if (!added && (id.index >> 16) == 0 && (identity(kept->second).index >> 16) != 0) kept->second = member;
  }
}

bool FaceReader::lists(FcPattern* pattern) {
  const Identity id = resource(identity(pattern));
  // The coordinates are part of every face's identity; a request without them has no identity to keep unique, and
  // reading the resource for it would be work no requested field needs.
  if (!request_.wants(Field::FacesCoordinates) || !variableResources_.contains(id)) return true;
  if (readResource(id).tables.fvar) return true;
  return keptWithoutFvar_.at(id) == pattern;
}

Face FaceReader::read(FcPattern* pattern, bool inApplicationSet) {
  const Identity id = identity(pattern);
  Face face;
  face.postscriptName = string(pattern, FC_POSTSCRIPT_NAME).value_or("");
  face.variable = variableResources_.contains(resource(id));
  face.file = FileLocation{id.path, id.index & 0xffff};
  face.familyNames = names(pattern, FC_FAMILY, FC_FAMILYLANG);
  face.fullNames = names(pattern, FC_FULLNAME, FC_FULLNAMELANG);
  face.styleNames = names(pattern, FC_STYLE, FC_STYLELANG);
  if (auto weight = number(pattern, FC_WEIGHT)) face.weight = FcWeightToOpenTypeDouble(*weight);
  if (auto width = number(pattern, FC_WIDTH)) face.width = *width;
  const int slant = integer(pattern, FC_SLANT).value_or(FC_SLANT_ROMAN);
  face.style = slant == FC_SLANT_ITALIC ? Style::Italic : slant == FC_SLANT_OBLIQUE ? Style::Oblique : Style::Normal;
  const int spacing = integer(pattern, FC_SPACING).value_or(FC_PROPORTIONAL);
  face.monospace = spacing == FC_MONO || spacing == FC_DUAL || spacing == FC_CHARCELL;
  face.color = boolean(pattern, FC_COLOR).value_or(false);
  face.symbol = boolean(pattern, FC_SYMBOL).value_or(false);
  face.scope = scopeOf(id.path, inApplicationSet);
  face.format = formatOf(string(pattern, FC_FONTFORMAT).value_or(""));
  if (request_.platformObject) face.platform = platformValue(pattern);
  if (request_.wants(Field::FacesSupportedLanguages)) {
    FcLangSet* languages = nullptr;
    if (FcPatternGetLangSet(pattern, FC_LANG, 0, &languages) == FcResultMatch) {
      Owned<FcStrSet, FcStrSetDestroy> strings(FcLangSetGetLangs(languages));
      require(bool(strings), "FcLangSetGetLangs");
      StrList list(FcStrListCreate(strings.get()));
      require(bool(list), "FcStrListCreate");
      while (auto value = FcStrListNext(list.get()))
        face.supportedLanguages.emplace_back(reinterpret_cast<const char*>(value));
    }
  }
  const bool needsVariations =
      face.variable && (request_.wants(Field::FacesCoordinates) || request_.wants(Field::FacesNamedInstanceIndex) ||
                        request_.wants(Field::FacesFullNames) || request_.wants(Field::FacesStyleNames));
  const bool needsCff2 = face.format == Format::Cff && request_.wants(Field::FacesFormat);
  if (request_.needs(Group::C) || request_.wantsAnyOpenType() || needsVariations || needsCff2) {
    const auto& data = readResource(resource(id));
    face.opentype = data.opentype;
    if (needsCff2 && data.tables.hasCff2) face.format = Format::Cff2;
    // Without a readable `fvar` that matches the face's index, a variable face has no axes and no coordinates.
    const uint32_t packedInstance = id.index >> 16;
    if (face.variable && (needsVariations || request_.wants(Field::FacesAxes)) && data.tables.fvar &&
        packedInstance <= data.tables.fvar->instances.size()) {
      const auto& fvar = *data.tables.fvar;
      std::vector<double> coordinates;
      if (packedInstance) {
        coordinates = fvar.instances[packedInstance - 1].coordinates;
      } else {
        for (const auto& axis : fvar.axes) coordinates.push_back(axis.defaultValue);
      }
      for (size_t i = 0; i < fvar.axes.size(); ++i)
        face.coordinates.emplace_back(sfnt::tagToString(fvar.axes[i].tag), coordinates[i]);
      face.axes = data.axes;
      for (size_t i = 0; i < fvar.instances.size(); ++i) {
        if (fvar.instances[i].coordinates != coordinates) continue;
        face.namedInstanceIndex = uint32_t(i);
        if (request_.wants(Field::FacesFullNames))
          face.fullNames = sfnt::instanceFullNames(data.tables.names, fvar.instances[i]);
        if (request_.wants(Field::FacesStyleNames))
          face.styleNames = sfnt::localizedName(data.tables.names, fvar.instances[i].subfamilyNameId);
        break;
      }
    }
  }
  if (request_.wants(Field::FacesCoverage)) {
    ++work_.characterSets;
    face.coverage = ranges(charset(pattern));
  }
  return face;
}
}  // namespace sfi::platform::linux_backend
