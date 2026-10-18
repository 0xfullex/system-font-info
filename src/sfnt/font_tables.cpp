#include <algorithm>
#include <array>
#include <set>

#include "../core/field_catalogue.h"
#include "internal.h"

namespace sfi::sfnt {

std::string tagToString(uint32_t tag) {
  const uint8_t bytes[4] = {uint8_t(tag >> 24), uint8_t(tag >> 16), uint8_t(tag >> 8), uint8_t(tag)};
  return latin1ToUtf8(bytes);
}

namespace {

// The `opentype` entries are the catalogue's `faces.opentype.<entry>` fields: the catalogue holds each entry's name
// and the tables it is made of. This array only says which field is which enumerator.
constexpr std::array<Field, size_t(OpenTypeEntry::Count_)> kEntryFields = {
    Field::FacesOpentypeTables, Field::FacesOpentypeNames, Field::FacesOpentypeHead, Field::FacesOpentypeHhea,
    Field::FacesOpentypeVhea,   Field::FacesOpentypeOs2,   Field::FacesOpentypePost, Field::FacesOpentypeMaxp,
    Field::FacesOpentypeCmap,   Field::FacesOpentypeFvar,  Field::FacesOpentypeMeta, Field::FacesOpentypeLayout,
};

constexpr const FieldInfo& entryInfo(OpenTypeEntry entry) { return kFields[size_t(kEntryFields[size_t(entry)])]; }

constexpr std::string_view kEntryNamePrefix = "faces.opentype.";

constexpr bool entryFieldsAreOpenTypeEntries() {
  for (size_t i = 0; i < kEntryFields.size(); ++i) {
    const FieldInfo& info = entryInfo(OpenTypeEntry(i));
    if (info.level != Level::OpenType || !info.name.starts_with(kEntryNamePrefix)) return false;
  }
  return true;
}
static_assert(entryFieldsAreOpenTypeEntries(), "OpenTypeEntry and the catalogue's opentype fields must correspond");
// The catalogue writes tags in the encoding of makeTag.
static_assert(entryInfo(OpenTypeEntry::Layout).tables[0] == makeTag("GSUB") &&
                  entryInfo(OpenTypeEntry::Layout).tables[1] == makeTag("GPOS"),
              "the catalogue and sfnt must encode table tags alike");

// Fetches a present table and runs `convert` on its bytes. A table that is
// absent gives nullopt without an error; one that cannot be fetched or parsed
// gives nullopt and is recorded in `failed`.
template <typename Convert>
auto readTable(const std::vector<uint32_t>& presentTags, const TableFetcher& fetch, std::set<uint32_t>& failed,
               uint32_t tag, Convert&& convert) -> std::optional<decltype(convert(std::span<const uint8_t>()))> {
  if (!std::binary_search(presentTags.begin(), presentTags.end(), tag)) return std::nullopt;
  const std::optional<std::vector<uint8_t>> bytes = fetch(tag);
  if (bytes) {
    try {
      return convert(std::span<const uint8_t>(*bytes));
    } catch (const ParseError&) {
    }
  }
  failed.insert(tag);
  return std::nullopt;
}

}  // namespace

std::optional<OpenTypeEntry> openTypeEntryFromName(std::string_view name) {
  for (size_t i = 0; i < size_t(OpenTypeEntry::Count_); ++i) {
    if (entryInfo(OpenTypeEntry(i)).name.substr(kEntryNamePrefix.size()) == name) return OpenTypeEntry(i);
  }
  return std::nullopt;
}

FontTables readFontTables(const std::vector<uint32_t>& tags, const TableFetcher& fetch,
                          const LegacyDecoder& decodeLegacy) {
  FontTables out;
  out.tags = tags;
  std::sort(out.tags.begin(), out.tags.end());
  out.tags.erase(std::unique(out.tags.begin(), out.tags.end()), out.tags.end());
  out.hasCff2 = std::binary_search(out.tags.begin(), out.tags.end(), makeTag("CFF2"));

  const auto read = [&](uint32_t tag, auto&& convert) { return readTable(out.tags, fetch, out.failed, tag, convert); };

  if (auto names = read(makeTag("name"), [&](auto bytes) { return parseNameTable(bytes, decodeLegacy); })) {
    out.names = std::move(*names);
  }
  out.fvar = read(makeTag("fvar"), [](auto bytes) { return parseFvar(bytes); });
  out.cmapValue = read(makeTag("cmap"), [&](auto bytes) {
    bool symbol = false;
    Value value = cmapToValue(bytes, symbol);
    out.symbolCmap = symbol;
    return value;
  });
  return out;
}

Value::Shared openTypeValue(const FontTables& tables, const OpenTypeSelection& selection, const TableFetcher& fetch) {
  // The failures of `readFontTables` plus those of the tables read here.
  std::set<uint32_t> failed = tables.failed;
  const auto read = [&](OpenTypeEntry entry, uint32_t tag, auto&& convert) {
    return selection.has(entry) ? readTable(tables.tags, fetch, failed, tag, convert)
                                : std::optional<decltype(convert(std::span<const uint8_t>()))>();
  };

  const auto head = read(OpenTypeEntry::Head, makeTag("head"), headToValue);
  const auto hhea = read(OpenTypeEntry::Hhea, makeTag("hhea"), horizontalHeaderToValue);
  const auto vhea = read(OpenTypeEntry::Vhea, makeTag("vhea"), horizontalHeaderToValue);
  const auto os2 = read(OpenTypeEntry::Os2, makeTag("OS/2"), os2ToValue);
  const auto post = read(OpenTypeEntry::Post, makeTag("post"), postToValue);
  const auto maxp = read(OpenTypeEntry::Maxp, makeTag("maxp"), maxpToValue);
  const auto meta = read(OpenTypeEntry::Meta, makeTag("meta"), metaToValue);
  const auto gsub =
      read(OpenTypeEntry::Layout, makeTag("GSUB"), [&](auto bytes) { return layoutToValue(bytes, tables.names); });
  const auto gpos =
      read(OpenTypeEntry::Layout, makeTag("GPOS"), [&](auto bytes) { return layoutToValue(bytes, tables.names); });

  Value opentype = Value::object();
  if (selection.has(OpenTypeEntry::Tables)) {
    Value tagList = Value::array();
    for (const uint32_t tag : tables.tags) tagList.push(tagToString(tag));
    opentype.set("tables", std::move(tagList));
  }
  if (selection.has(OpenTypeEntry::Names)) opentype.set("names", nameRecordsToValue(tables.names));

  std::set<uint32_t> errorTags;
  for (size_t i = 0; i < size_t(OpenTypeEntry::Count_); ++i) {
    const OpenTypeEntry entry = OpenTypeEntry(i);
    if (!selection.has(entry)) continue;
    for (const uint32_t tag : entryInfo(entry).tables) {
      if (failed.contains(tag)) errorTags.insert(tag);
    }
  }
  if (!errorTags.empty()) {
    Value errors = Value::array();
    for (const uint32_t tag : errorTags) errors.push(tagToString(tag));
    opentype.set("errors", std::move(errors));
  }

  if (head) opentype.set("head", *head);
  if (hhea) opentype.set("hhea", *hhea);
  if (vhea) opentype.set("vhea", *vhea);
  if (os2) opentype.set("os2", *os2);
  if (post) opentype.set("post", *post);
  if (maxp) opentype.set("maxp", *maxp);
  if (selection.has(OpenTypeEntry::Cmap) && tables.cmapValue) opentype.set("cmap", *tables.cmapValue);
  if (selection.has(OpenTypeEntry::Fvar) && tables.fvar) opentype.set("fvar", fvarToValue(*tables.fvar, tables.names));
  if (meta) opentype.set("meta", *meta);
  if (gsub || gpos) {
    Value layout = Value::object();
    if (gsub) layout.set("gsub", *gsub);
    if (gpos) layout.set("gpos", *gpos);
    opentype.set("layout", std::move(layout));
  }
  return std::make_shared<const Value>(std::move(opentype));
}

}  // namespace sfi::sfnt
