#include <algorithm>
#include <set>

#include "internal.h"

namespace sfi::sfnt {

std::string tagToString(uint32_t tag) {
  const uint8_t bytes[4] = {uint8_t(tag >> 24), uint8_t(tag >> 16), uint8_t(tag >> 8), uint8_t(tag)};
  return latin1ToUtf8(bytes);
}

FontTables readFontTables(const std::vector<uint32_t>& tags, const TableFetcher& fetch,
                          const LegacyDecoder& decodeLegacy) {
  FontTables out;
  out.tags = tags;
  std::sort(out.tags.begin(), out.tags.end());
  out.tags.erase(std::unique(out.tags.begin(), out.tags.end()), out.tags.end());
  const auto has = [&](uint32_t tag) { return std::binary_search(out.tags.begin(), out.tags.end(), tag); };
  out.hasCff2 = has(makeTag("CFF2"));

  // Tags of tables that are present but could not be read or parsed.
  std::set<uint32_t> failed;
  // Fetches a present table and runs `convert` on its bytes. A table that is
  // absent gives nullopt without an error; one that cannot be fetched or parsed
  // gives nullopt and is recorded in `failed`.
  const auto read = [&](uint32_t tag, auto&& convert) -> std::optional<decltype(convert(std::span<const uint8_t>()))> {
    if (!has(tag)) return std::nullopt;
    const std::optional<std::vector<uint8_t>> bytes = fetch(tag);
    if (bytes) {
      try {
        return convert(std::span<const uint8_t>(*bytes));
      } catch (const ParseError&) {
      }
    }
    failed.insert(tag);
    return std::nullopt;
  };

  // The name table comes first: fvar and the layout tables look names up in it.
  if (auto names = read(makeTag("name"), [&](auto bytes) { return parseNameTable(bytes, decodeLegacy); })) {
    out.names = std::move(*names);
  }
  out.fvar = read(makeTag("fvar"), [](auto bytes) { return parseFvar(bytes); });

  const auto head = read(makeTag("head"), headToValue);
  const auto hhea = read(makeTag("hhea"), horizontalHeaderToValue);
  const auto vhea = read(makeTag("vhea"), horizontalHeaderToValue);
  const auto os2 = read(makeTag("OS/2"), os2ToValue);
  const auto post = read(makeTag("post"), postToValue);
  const auto maxp = read(makeTag("maxp"), maxpToValue);
  const auto cmap = read(makeTag("cmap"), [&](auto bytes) {
    bool symbol = false;
    Value value = cmapToValue(bytes, symbol);
    out.symbolCmap = symbol;
    return value;
  });
  const auto meta = read(makeTag("meta"), metaToValue);
  const auto gsub = read(makeTag("GSUB"), [&](auto bytes) { return layoutToValue(bytes, out.names); });
  const auto gpos = read(makeTag("GPOS"), [&](auto bytes) { return layoutToValue(bytes, out.names); });

  Value tagList = Value::array();
  for (const uint32_t tag : out.tags) tagList.push(tagToString(tag));

  Value opentype = Value::object();
  opentype.set("tables", std::move(tagList)).set("names", nameRecordsToValue(out.names));
  if (!failed.empty()) {
    Value errors = Value::array();
    for (const uint32_t tag : failed) errors.push(tagToString(tag));
    opentype.set("errors", std::move(errors));
  }
  if (head) opentype.set("head", *head);
  if (hhea) opentype.set("hhea", *hhea);
  if (vhea) opentype.set("vhea", *vhea);
  if (os2) opentype.set("os2", *os2);
  if (post) opentype.set("post", *post);
  if (maxp) opentype.set("maxp", *maxp);
  if (cmap) opentype.set("cmap", *cmap);
  if (out.fvar) opentype.set("fvar", fvarToValue(*out.fvar, out.names));
  if (meta) opentype.set("meta", *meta);
  if (gsub || gpos) {
    Value layout = Value::object();
    if (gsub) layout.set("gsub", *gsub);
    if (gpos) layout.set("gpos", *gpos);
    opentype.set("layout", std::move(layout));
  }
  out.opentype = std::make_shared<const Value>(std::move(opentype));
  return out;
}

}  // namespace sfi::sfnt
