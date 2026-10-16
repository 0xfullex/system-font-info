// The fixed-layout tables (head, hhea/vhea, OS/2, post, maxp) plus cmap and
// meta, converted to the Value shapes of OpenTypeInfo in ts/types.ts. Numbers
// are as stored in the font; only dates, versions, text and flags-as-boolean
// are interpreted.
#include <cstdio>
#include <optional>

#include "internal.h"
#include "reader.h"

namespace sfi::sfnt {

namespace {

// Version16Dot16: major in the high word; the minor word counts tenths in its
// top hex digit (0x00005000 is 0.5, 0x00011000 is 1.1, 0x00025000 is 2.5). A
// minor word that is not of that form (0x00010001) is read as a plain fraction
// of 65536 so that no bits are lost.
double version16Dot16(const Reader& r, size_t offset) {
  const uint16_t minor = r.u16(offset + 2);
  return r.u16(offset) + ((minor & 0x0FFF) == 0 ? (minor >> 12) / 10.0 : minor / 65536.0);
}

// Seconds since 1904-01-01 00:00 UTC -> "YYYY-MM-DDTHH:MM:SSZ". The conversion
// is literal; a result whose year is outside 0001-9999 (a garbage timestamp, or
// a Unix timestamp the font wrote by mistake) has no four-digit form and gives
// nullopt, so the date is left out instead of being reported as something odd.
std::optional<std::string> isoFromLongDateTime(int64_t seconds) {
  constexpr int64_t kSecondsPerDay = 86400;
  constexpr int64_t kDaysFrom1904To1970 = 24107;
  int64_t days = seconds / kSecondsPerDay;
  int64_t secondOfDay = seconds % kSecondsPerDay;
  if (secondOfDay < 0) {
    secondOfDay += kSecondsPerDay;
    --days;
  }
  days -= kDaysFrom1904To1970;

  // Days since 1970-01-01 -> proleptic Gregorian date (H. Hinnant's civil_from_days).
  days += 719468;
  const int64_t era = (days >= 0 ? days : days - 146096) / 146097;
  const int64_t dayOfEra = days - era * 146097;
  const int64_t yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
  const int64_t dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
  const int64_t mp = (5 * dayOfYear + 2) / 153;
  const int day = int(dayOfYear - (153 * mp + 2) / 5 + 1);
  const int month = int(mp < 10 ? mp + 3 : mp - 9);
  const int64_t year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);

  if (year < 1 || year > 9999) return std::nullopt;

  char text[32];
  const int hour = int(secondOfDay / 3600);
  const int minute = int(secondOfDay % 3600 / 60);
  const int second = int(secondOfDay % 60);
  std::snprintf(text, sizeof text, "%04d-%02d-%02dT%02d:%02d:%02dZ", int(year), month, day, hour, minute, second);
  return std::string(text);
}

// Latin-1 so that every byte survives as a valid UTF-8 character.
std::string fourCharString(const Reader& r, size_t offset) { return latin1ToUtf8(r.bytes(offset, 4)); }

Value subscriptValue(const Reader& r, size_t at) {
  Value v = Value::object();
  v.set("xSize", r.i16(at)).set("ySize", r.i16(at + 2)).set("xOffset", r.i16(at + 4)).set("yOffset", r.i16(at + 6));
  return v;
}

}  // namespace

Value headToValue(std::span<const uint8_t> table) {
  const Reader r(table);
  Value boundingBox = Value::array();
  for (size_t i = 0; i < 4; ++i) boundingBox.push(r.i16(36 + i * 2));
  Value v = Value::object();
  v.set("version", version16Dot16(r, 0))
      .set("fontRevision", r.fixed(4))
      .set("flags", r.u16(16))
      .set("unitsPerEm", r.u16(18));
  if (const auto created = isoFromLongDateTime(r.i64(20))) v.set("created", *created);
  if (const auto modified = isoFromLongDateTime(r.i64(28))) v.set("modified", *modified);
  v.set("boundingBox", std::move(boundingBox))
      .set("macStyle", r.u16(44))
      .set("lowestRecPPEM", r.u16(46))
      .set("fontDirectionHint", r.i16(48))
      .set("indexToLocFormat", r.i16(50))
      .set("glyphDataFormat", r.i16(52));
  return v;
}

Value horizontalHeaderToValue(std::span<const uint8_t> table) {
  const Reader r(table);
  Value v = Value::object();
  v.set("version", version16Dot16(r, 0))
      .set("ascender", r.i16(4))
      .set("descender", r.i16(6))
      .set("lineGap", r.i16(8))
      .set("advanceMax", r.u16(10))
      .set("minLeadingBearing", r.i16(12))
      .set("minTrailingBearing", r.i16(14))
      .set("maxExtent", r.i16(16))
      .set("caretSlopeRise", r.i16(18))
      .set("caretSlopeRun", r.i16(20))
      .set("caretOffset", r.i16(22))
      .set("metricDataFormat", r.i16(32))
      .set("numberOfMetrics", r.u16(34));
  return v;
}

Value os2ToValue(std::span<const uint8_t> table) {
  // Offsets where each version's additions begin; the table must hold the
  // version-0 part. A newer-version field group is read only when the table is
  // long enough for all of it: fonts that declare a version but stop short exist.
  constexpr size_t kVersion1 = 78;
  constexpr size_t kVersion2 = 86;
  constexpr size_t kVersion5 = 96;
  constexpr size_t kEnd = 100;

  const Reader r(table);
  const uint16_t version = r.u16(0);
  Value panose = Value::array();
  for (size_t i = 0; i < 10; ++i) panose.push(r.u8(32 + i));
  Value unicodeRanges = Value::array();
  for (size_t i = 0; i < 4; ++i) unicodeRanges.push(r.u32(42 + i * 4));
  Value strikeout = Value::object();
  strikeout.set("size", r.i16(26)).set("position", r.i16(28));

  Value v = Value::object();
  v.set("version", version)
      .set("xAvgCharWidth", r.i16(2))
      .set("weightClass", r.u16(4))
      .set("widthClass", r.u16(6))
      .set("fsType", r.u16(8))
      .set("subscript", subscriptValue(r, 10))
      .set("superscript", subscriptValue(r, 18))
      .set("strikeout", std::move(strikeout))
      .set("familyClass", r.i16(30))
      .set("panose", std::move(panose))
      .set("unicodeRanges", std::move(unicodeRanges))
      .set("vendorId", fourCharString(r, 58))
      .set("fsSelection", r.u16(62))
      .set("firstCharIndex", r.u16(64))
      .set("lastCharIndex", r.u16(66))
      .set("typoAscender", r.i16(68))
      .set("typoDescender", r.i16(70))
      .set("typoLineGap", r.i16(72))
      .set("winAscent", r.u16(74))
      .set("winDescent", r.u16(76));

  if (version >= 1 && table.size() >= kVersion2) {
    Value codePageRanges = Value::array();
    codePageRanges.push(r.u32(kVersion1)).push(r.u32(kVersion1 + 4));
    v.set("codePageRanges", std::move(codePageRanges));
  }
  if (version >= 2 && table.size() >= kVersion5) {
    v.set("xHeight", r.i16(kVersion2))
        .set("capHeight", r.i16(kVersion2 + 2))
        .set("defaultChar", r.u16(kVersion2 + 4))
        .set("breakChar", r.u16(kVersion2 + 6))
        .set("maxContext", r.u16(kVersion2 + 8));
  }
  if (version >= 5 && table.size() >= kEnd) {
    // Stored in twentieths of a point (TWIPs).
    v.set("lowerOpticalPointSize", r.u16(kVersion5) / 20.0).set("upperOpticalPointSize", r.u16(kVersion5 + 2) / 20.0);
  }
  return v;
}

Value postToValue(std::span<const uint8_t> table) {
  const Reader r(table);
  Value v = Value::object();
  v.set("version", version16Dot16(r, 0))
      .set("italicAngle", r.fixed(4))
      .set("underlinePosition", r.i16(8))
      .set("underlineThickness", r.i16(10))
      .set("isFixedPitch", r.u32(12) != 0);
  return v;
}

Value maxpToValue(std::span<const uint8_t> table) {
  const Reader r(table);
  Value v = Value::object();
  v.set("version", version16Dot16(r, 0)).set("numGlyphs", r.u16(4));
  return v;
}

Value cmapToValue(std::span<const uint8_t> table, bool& hasSymbolSubtable) {
  constexpr uint16_t kWindows = 3;
  constexpr uint16_t kWindowsSymbol = 0;
  const Reader r(table);
  const size_t count = r.u16(2);
  r.require(4, count * 8);
  Value subtables = Value::array();
  for (size_t i = 0; i < count; ++i) {
    const size_t at = 4 + i * 8;
    const uint16_t platformId = r.u16(at);
    const uint16_t encodingId = r.u16(at + 2);
    if (platformId == kWindows && encodingId == kWindowsSymbol) hasSymbolSubtable = true;
    Value item = Value::object();
    item.set("platformId", platformId).set("encodingId", encodingId).set("format", r.u16(r.u32(at + 4)));
    subtables.push(std::move(item));
  }
  Value v = Value::object();
  v.set("subtables", std::move(subtables));
  return v;
}

Value metaToValue(std::span<const uint8_t> table) {
  constexpr size_t kHeaderSize = 16;
  constexpr size_t kDataMapSize = 12;
  const Reader r(table);
  const size_t count = r.u32(12);
  r.require(kHeaderSize, count * kDataMapSize);

  // `dlng` and `slng` are UTF-8 lists of ScriptLangTag separated by commas.
  std::optional<std::vector<std::string>> designLanguages;
  std::optional<std::vector<std::string>> supportedLanguages;
  Value tags = Value::array();
  for (size_t i = 0; i < count; ++i) {
    const size_t at = kHeaderSize + i * kDataMapSize;
    const uint32_t tag = r.u32(at);
    tags.push(tagToString(tag));
    const bool isDesign = tag == makeTag("dlng");
    const bool isSupported = tag == makeTag("slng");
    if (!isDesign && !isSupported) continue;

    const std::span<const uint8_t> data = r.bytes(r.u32(at + 4), r.u32(at + 8));
    std::optional<std::vector<std::string>>& list = isDesign ? designLanguages : supportedLanguages;
    if (!list) list.emplace();
    std::string item;
    auto flush = [&] {
      const size_t first = item.find_first_not_of(" \t\r\n");
      if (first != std::string::npos) list->push_back(item.substr(first, item.find_last_not_of(" \t\r\n") - first + 1));
      item.clear();
    };
    for (const uint8_t c : data) {
      if (c == ',') {
        flush();
      } else {
        item += char(c);
      }
    }
    flush();
  }

  auto toArray = [](const std::vector<std::string>& list) {
    Value out = Value::array();
    for (const std::string& s : list) out.push(s);
    return out;
  };
  Value v = Value::object();
  if (designLanguages) v.set("dlng", toArray(*designLanguages));
  if (supportedLanguages) v.set("slng", toArray(*supportedLanguages));
  v.set("tags", std::move(tags));
  return v;
}

}  // namespace sfi::sfnt
