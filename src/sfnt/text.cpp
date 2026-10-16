#include "internal.h"

namespace sfi::sfnt {

namespace {

void appendUtf8(std::string& out, char32_t cp) {
  if (cp < 0x80) {
    out += char(cp);
  } else if (cp < 0x800) {
    out += char(0xC0 | (cp >> 6));
    out += char(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += char(0xE0 | (cp >> 12));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  } else {
    out += char(0xF0 | (cp >> 18));
    out += char(0x80 | ((cp >> 12) & 0x3F));
    out += char(0x80 | ((cp >> 6) & 0x3F));
    out += char(0x80 | (cp & 0x3F));
  }
}

constexpr char32_t kReplacement = 0xFFFD;

}  // namespace

std::string utf16beToUtf8(std::span<const uint8_t> bytes) {
  std::string out;
  out.reserve(bytes.size());
  const size_t units = bytes.size() / 2;
  auto unitAt = [&](size_t i) { return char32_t((bytes[2 * i] << 8) | bytes[2 * i + 1]); };
  for (size_t i = 0; i < units; ++i) {
    const char32_t unit = unitAt(i);
    if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < units && unitAt(i + 1) >= 0xDC00 && unitAt(i + 1) <= 0xDFFF) {
      appendUtf8(out, 0x10000 + ((unit - 0xD800) << 10) + (unitAt(i + 1) - 0xDC00));
      ++i;
    } else if (unit >= 0xD800 && unit <= 0xDFFF) {
      appendUtf8(out, kReplacement);
    } else {
      appendUtf8(out, unit);
    }
  }
  if (bytes.size() % 2 != 0) appendUtf8(out, kReplacement);
  return out;
}

std::string latin1ToUtf8(std::span<const uint8_t> bytes) {
  std::string out;
  out.reserve(bytes.size());
  for (const uint8_t b : bytes) appendUtf8(out, b);
  return out;
}

std::string toHex(std::span<const uint8_t> bytes) {
  static constexpr char kDigits[] = "0123456789abcdef";
  std::string out;
  out.reserve(bytes.size() * 2);
  for (const uint8_t b : bytes) {
    out += kDigits[b >> 4];
    out += kDigits[b & 0x0F];
  }
  return out;
}

}  // namespace sfi::sfnt
