#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "../core/model.h"
#include "../core/value.h"
#include "legacy_encoding.h"

// A platform-independent reader for the standard OpenType tables. Every
// backend feeds it table bytes (from CTFontCopyTable, IDWriteFontFace::
// TryGetFontTable, or the file itself), so all platforms report the font's own
// metadata identically.

namespace sfi::sfnt {

constexpr uint32_t makeTag(const char (&s)[5]) {
  return (uint32_t(uint8_t(s[0])) << 24) | (uint32_t(uint8_t(s[1])) << 16) | (uint32_t(uint8_t(s[2])) << 8) |
         uint32_t(uint8_t(s[3]));
}
std::string tagToString(uint32_t tag);

// Thrown for truncated or inconsistent table data.
struct ParseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

// ---- name table ---------------------------------------------------------

// One record of the `name` table, as stored. `language` is the BCP 47 tag
// mapped from `languageId` ("" when no mapping exists; `languageId` keeps the
// raw value). `value` is the decoded text; when the encoding cannot be
// decoded (or is not one of LegacyEncoding), `value` is nullopt and `bytesHex`
// holds the raw bytes.
struct NameRecord {
  uint16_t platformId = 0;
  uint16_t encodingId = 0;
  uint16_t languageId = 0;
  uint16_t nameId = 0;
  std::string language;
  std::optional<std::string> value;
  std::string bytesHex;
};

// Turns bytes of a legacy encoding into UTF-8; supplied by the platform layer
// (platform::decodeLegacyText). sfnt chooses the encoding of each record, so the
// decoder only converts. Returns nullopt when it has no converter for the
// encoding or the bytes do not convert.
using LegacyDecoder = std::function<std::optional<std::string>(LegacyEncoding, std::span<const uint8_t>)>;

// Every record in table order, nothing merged. Handles format 0 and format 1
// (language-tag records). Throws ParseError on a malformed table.
std::vector<NameRecord> parseNameTable(std::span<const uint8_t> table, const LegacyDecoder& decodeLegacy);

// Every decoded record of one nameID, in table order. Duplicates are kept: the
// JS layer merges them.
LocalizedStrings localizedName(const std::vector<NameRecord>& records, uint16_t nameId);

// Records of `preferred` (e.g. 16, typographic family), plus records of
// `fallback` (e.g. 1, legacy family) in every language `preferred` lacks.
// Two tags are the same language when their primary subtags match, so a Mac
// "en" record of the fallback does not join a Windows "en-US" record of the
// preferred ID. Chinese is told apart by script: "zh-Hans" and "zh-CN" and
// "zh-SG" are one language, "zh-Hant", "zh-TW", "zh-HK" and "zh-MO" another,
// and a plain "zh" a third.
LocalizedStrings localizedNameWithFallback(const std::vector<NameRecord>& records, uint16_t preferred,
                                           uint16_t fallback);

// Windows LCID / Macintosh language code -> BCP 47; "" when unknown.
std::string lcidToBcp47(uint16_t lcid);
std::string macLanguageToBcp47(uint16_t code);

// ---- fvar -----------------------------------------------------------------

struct FvarAxis {
  uint32_t tag = 0;
  double min = 0;
  double defaultValue = 0;
  double max = 0;
  uint16_t flags = 0;  // bit 0: HIDDEN_AXIS
  uint16_t nameId = 0;
};

struct FvarInstance {
  uint16_t subfamilyNameId = 0;
  uint16_t flags = 0;
  std::vector<double> coordinates;  // one per axis, axis order
  std::optional<uint16_t> postscriptNameId;
};

struct Fvar {
  std::vector<FvarAxis> axes;
  std::vector<FvarInstance> instances;
};

// The full names of a named instance, which has no full-name record of its
// own: for each record of the instance's subfamily name (`subfamilyNameId`),
// the typographic family name (nameID 16, else 1, as localizedNameWithFallback
// reads it) in the same language, a space, and the subfamily name. This is the
// "combination of name IDs 16 and 17" the OpenType `name` specification
// describes for name ID 4, where the subfamily name of an instance is
// "equivalent to name ID 17" (`fvar`); a "Regular" subfamily is not left out.
// "Same language" is the rule of localizedNameWithFallback; a subfamily record
// takes the family record of its own language when there is one, else the
// first of the same language. A subfamily record without a family name of its
// language gives no full name. The result carries the subfamily record's
// language, in table order.
LocalizedStrings instanceFullNames(const std::vector<NameRecord>& records, const FvarInstance& instance);

// Throws ParseError on a malformed table.
Fvar parseFvar(std::span<const uint8_t> table);

// ---- whole font -----------------------------------------------------------

// Returns one table's bytes, or nullopt when the font has no such table.
using TableFetcher = std::function<std::optional<std::vector<uint8_t>>(uint32_t tag)>;

struct FontTables {
  std::vector<uint32_t> tags;     // tables present, ascending
  std::vector<NameRecord> names;  // empty when there is no usable name table
  std::optional<Fvar> fvar;
  bool symbolCmap = false;  // cmap has a (3,0) subtable
  bool hasCff2 = false;
  Value::Shared opentype;  // the `opentype` object of a face (see ts/types.ts OpenTypeInfo)
};

// Reads and parses the standard tables listed in OpenTypeInfo. A table that
// is present but malformed is left out of `opentype` and its tag is listed in
// `opentype.errors`; this function itself does not throw for table content.
FontTables readFontTables(const std::vector<uint32_t>& tags, const TableFetcher& fetch,
                          const LegacyDecoder& decodeLegacy);

// ---- font files -----------------------------------------------------------

// Direct access to an sfnt file or a collection (TTC/OTC): used where the OS
// gives no table access, and to find a face's index inside a collection.
class FontFile {
 public:
  // nullopt when the file cannot be read or is not an sfnt / collection.
  static std::optional<FontFile> open(const std::string& path);

  size_t faceCount() const;
  std::vector<uint32_t> tableTags(size_t faceIndex) const;
  std::optional<std::vector<uint8_t>> table(size_t faceIndex, uint32_t tag) const;

  // The first face that has `postscriptName`: as its own PostScript name
  // (nameID 6) or as the PostScript name of one of its fvar named instances
  // (the record of the instance's postScriptNameID). Names are compared as
  // decoded by parseNameTable with `decodeLegacy`, in any language. A face whose
  // name or fvar table is missing or malformed does not match.
  std::optional<size_t> faceWithPostscriptName(const std::string& postscriptName,
                                               const LegacyDecoder& decodeLegacy) const;

 private:
  bool hasPostscriptName(size_t faceIndex, const std::string& postscriptName, const LegacyDecoder& decodeLegacy) const;

  struct TableRecord {
    uint32_t tag;
    uint32_t offset;
    uint32_t length;
  };
  std::string path_;
  std::vector<std::vector<TableRecord>> faces_;
};

}  // namespace sfi::sfnt
