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
// decoded, `value` is nullopt and `bytesHex` holds the raw bytes.
struct NameRecord {
  uint16_t platformId = 0;
  uint16_t encodingId = 0;
  uint16_t languageId = 0;
  uint16_t nameId = 0;
  std::string language;
  std::optional<std::string> value;
  std::string bytesHex;
};

// Decodes strings in encodings other than UTF-16BE; supplied by the platform
// layer (platform::decodeLegacyName). Returns nullopt when unsupported.
using LegacyDecoder = std::function<std::optional<std::string>(uint16_t platformId, uint16_t encodingId,
                                                               uint16_t languageId, std::span<const uint8_t> bytes)>;

// Every record in table order, nothing merged. Handles format 0 and format 1
// (language-tag records). Throws ParseError on a malformed table.
std::vector<NameRecord> parseNameTable(std::span<const uint8_t> table, const LegacyDecoder& decodeLegacy);

// The merged view of one nameID: decoded records only, exact (language, value)
// duplicates removed, table order kept.
LocalizedStrings localizedName(const std::vector<NameRecord>& records, uint16_t nameId);

// Records of `preferred` (e.g. 16, typographic family), plus records of
// `fallback` (e.g. 1, legacy family) in every language `preferred` lacks.
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

// Throws ParseError on a malformed table.
Fvar parseFvar(std::span<const uint8_t> table);

// ---- whole font -----------------------------------------------------------

// Returns one table's bytes, or nullopt when the font has no such table.
using TableFetcher = std::function<std::optional<std::vector<uint8_t>>(uint32_t tag)>;

struct FontTables {
  std::vector<uint32_t> tags;     // tables present, ascending
  std::vector<NameRecord> names;  // empty when there is no usable name table
  std::optional<Fvar> fvar;
  bool symbolCmap = false;        // cmap has a (3,0) subtable
  bool hasCff2 = false;
  Value::Shared opentype;         // the `opentype` object of a face (see ts/types.ts OpenTypeInfo)
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

 private:
  struct TableRecord {
    uint32_t tag;
    uint32_t offset;
    uint32_t length;
  };
  std::string path_;
  std::vector<std::vector<TableRecord>> faces_;
};

}  // namespace sfi::sfnt
