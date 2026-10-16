#include <algorithm>
#include <cctype>

#include "internal.h"
#include "language_tables.h"
#include "reader.h"

namespace sfi::sfnt {

namespace {

constexpr uint16_t kPlatformUnicode = 0;
constexpr uint16_t kPlatformMacintosh = 1;
constexpr uint16_t kPlatformIso = 2;
constexpr uint16_t kPlatformWindows = 3;
constexpr uint16_t kFirstLanguageTagId = 0x8000;

// UTF-16BE and the two ISO encodings are decoded here. Every other encoding
// (Macintosh, and the East Asian Windows code pages) goes to the platform's
// LegacyDecoder, which knows those code pages.
std::optional<std::string> decodeName(const NameRecord& record, std::span<const uint8_t> bytes,
                                      const LegacyDecoder& decodeLegacy) {
  const uint16_t platform = record.platformId;
  const uint16_t encoding = record.encodingId;
  if (platform == kPlatformUnicode || (platform == kPlatformWindows && (encoding == 0 || encoding == 1 || encoding == 10)) ||
      (platform == kPlatformIso && encoding == 1)) {
    return utf16beToUtf8(bytes);
  }
  if (platform == kPlatformIso && encoding == 2) return latin1ToUtf8(bytes);
  if (platform == kPlatformIso && encoding == 0) {
    // 7-bit ASCII: a byte above 0x7F has no meaning, so the record is left undecoded.
    if (std::any_of(bytes.begin(), bytes.end(), [](uint8_t b) { return b >= 0x80; })) return std::nullopt;
    return std::string(bytes.begin(), bytes.end());
  }

  if (bytes.empty()) return std::string();
  std::optional<std::string> value;
  if (decodeLegacy) value = decodeLegacy(platform, encoding, record.languageId, bytes);
  if (value && value->empty()) return std::nullopt;
  return value;
}

std::string lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
  return text;
}

// What "the same language" means when the legacy family and style names
// (nameIDs 1 and 2) stand in for the typographic ones (16 and 17). The two sets
// come from different platforms, whose tags for one language differ in their
// region ("en" on a Mac, "en-US" on Windows), so the key is the primary subtag.
// Chinese is the exception: Simplified and Traditional are different written
// languages that a font names separately, so its key also carries the script:
// an explicit Hans or Hant, else the one the region implies (CN and SG are
// Simplified; TW, HK and MO are Traditional), else plain "zh".
std::string languageKey(const std::string& tag) {
  std::vector<std::string> subtags;
  size_t start = 0;
  while (start <= tag.size()) {
    size_t end = tag.find_first_of("-_", start);
    if (end == std::string::npos) end = tag.size();
    subtags.push_back(lowercase(tag.substr(start, end - start)));
    start = end + 1;
  }
  if (subtags[0] != "zh") return subtags[0];

  std::string impliedByRegion;
  for (size_t i = 1; i < subtags.size(); ++i) {
    const std::string& subtag = subtags[i];
    if (subtag == "hans") return "zh-Hans";
    if (subtag == "hant") return "zh-Hant";
    if (subtag == "cn" || subtag == "sg") impliedByRegion = "zh-Hans";
    if (subtag == "tw" || subtag == "hk" || subtag == "mo") impliedByRegion = "zh-Hant";
  }
  return impliedByRegion.empty() ? "zh" : impliedByRegion;
}

}  // namespace

std::string lcidToBcp47(uint16_t lcid) {
  const LcidEntry* end = kLcidTable + kLcidTableSize;
  const LcidEntry* it = std::lower_bound(kLcidTable, end, lcid, [](const LcidEntry& e, uint16_t key) { return e.lcid < key; });
  return it != end && it->lcid == lcid ? it->tag : "";
}

std::string macLanguageToBcp47(uint16_t code) {
  const MacLanguageEntry* end = kMacLanguageTable + kMacLanguageTableSize;
  const MacLanguageEntry* it =
      std::lower_bound(kMacLanguageTable, end, code, [](const MacLanguageEntry& e, uint16_t key) { return e.code < key; });
  return it != end && it->code == code ? it->tag : "";
}

std::vector<NameRecord> parseNameTable(std::span<const uint8_t> table, const LegacyDecoder& decodeLegacy) {
  constexpr size_t kHeaderSize = 6;
  constexpr size_t kRecordSize = 12;

  const Reader r(table);
  const uint16_t format = r.u16(0);
  if (format > 1) throw ParseError("unknown name table format");
  const size_t count = r.u16(2);
  const size_t storageOffset = r.u16(4);
  r.require(kHeaderSize, count * kRecordSize);

  // Format 1: language-tag records follow the name records; their strings are
  // UTF-16BE and live in the same string storage.
  std::vector<std::string> languageTags;
  if (format == 1) {
    const size_t tagsOffset = kHeaderSize + count * kRecordSize;
    const size_t tagCount = r.u16(tagsOffset);
    r.require(tagsOffset + 2, tagCount * 4);
    for (size_t i = 0; i < tagCount; ++i) {
      const size_t length = r.u16(tagsOffset + 2 + i * 4);
      const size_t offset = r.u16(tagsOffset + 4 + i * 4);
      languageTags.push_back(utf16beToUtf8(r.bytes(storageOffset + offset, length)));
    }
  }

  std::vector<NameRecord> records;
  records.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    const size_t at = kHeaderSize + i * kRecordSize;
    NameRecord record;
    record.platformId = r.u16(at);
    record.encodingId = r.u16(at + 2);
    record.languageId = r.u16(at + 4);
    record.nameId = r.u16(at + 6);
    const size_t length = r.u16(at + 8);
    const size_t offset = r.u16(at + 10);

    if (format == 1 && record.languageId >= kFirstLanguageTagId) {
      const size_t tagIndex = record.languageId - kFirstLanguageTagId;
      if (tagIndex < languageTags.size()) record.language = languageTags[tagIndex];
    } else if (record.platformId == kPlatformWindows) {
      record.language = lcidToBcp47(record.languageId);
    } else if (record.platformId == kPlatformMacintosh) {
      record.language = macLanguageToBcp47(record.languageId);
    }

    // A zero-length string needs no storage, and some fonts leave its offset arbitrary.
    const std::span<const uint8_t> bytes = length == 0 ? std::span<const uint8_t>() : r.bytes(storageOffset + offset, length);
    record.value = decodeName(record, bytes, decodeLegacy);
    if (!record.value) record.bytesHex = toHex(bytes);
    records.push_back(std::move(record));
  }
  return records;
}

LocalizedStrings localizedName(const std::vector<NameRecord>& records, uint16_t nameId) {
  LocalizedStrings out;
  for (const NameRecord& record : records) {
    if (record.nameId == nameId && record.value) out.push_back({record.language, *record.value});
  }
  return out;
}

LocalizedStrings localizedNameWithFallback(const std::vector<NameRecord>& records, uint16_t preferred,
                                           uint16_t fallback) {
  LocalizedStrings out = localizedName(records, preferred);
  const size_t preferredCount = out.size();
  for (const LocalizedString& candidate : localizedName(records, fallback)) {
    const std::string key = languageKey(candidate.language);
    const bool languageCovered = std::any_of(out.begin(), out.begin() + preferredCount, [&](const LocalizedString& s) {
      return languageKey(s.language) == key;
    });
    if (!languageCovered) out.push_back(candidate);
  }
  return out;
}

LocalizedStrings instanceFullNames(const std::vector<NameRecord>& records, const FvarInstance& instance) {
  const LocalizedStrings families = localizedNameWithFallback(records, 16, 1);
  LocalizedStrings out;
  for (const LocalizedString& subfamily : localizedName(records, instance.subfamilyNameId)) {
    const std::string key = languageKey(subfamily.language);
    const LocalizedString* family = nullptr;
    for (const LocalizedString& candidate : families) {
      if (candidate.language == subfamily.language) {
        family = &candidate;
        break;
      }
      if (!family && languageKey(candidate.language) == key) family = &candidate;
    }
    if (family) out.push_back({subfamily.language, family->value + " " + subfamily.value});
  }
  return out;
}

Value localizedStringsToValue(const LocalizedStrings& strings) {
  Value out = Value::array();
  for (const LocalizedString& s : strings) {
    Value item = Value::object();
    item.set("language", s.language).set("value", s.value);
    out.push(std::move(item));
  }
  return out;
}

Value nameRecordsToValue(const std::vector<NameRecord>& records) {
  Value out = Value::array();
  for (const NameRecord& record : records) {
    Value item = Value::object();
    item.set("platformId", record.platformId)
        .set("encodingId", record.encodingId)
        .set("languageId", record.languageId)
        .set("nameId", record.nameId)
        .set("language", record.language);
    if (record.value) {
      item.set("value", *record.value);
    } else {
      item.set("bytes", record.bytesHex);
    }
    out.push(std::move(item));
  }
  return out;
}

}  // namespace sfi::sfnt
