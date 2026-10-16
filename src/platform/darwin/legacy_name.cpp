#include <algorithm>
#include <iterator>
#include <vector>

#include "../../core/platform.h"
#include "cf.h"

// decodeLegacyName: `name` table strings that are not UTF-16BE, decoded with
// CoreFoundation's string encodings. The choice of encoding follows the rules
// of fontTools (ttLib/tables/_n_a_m_e.py and misc/encodingTools.py), the most
// widely used reader of such fonts, so both tools read old fonts the same way.

namespace sfi::platform {
namespace {

using darwin::Ref;

constexpr uint16_t kPlatformUnicode = 0;
constexpr uint16_t kPlatformMacintosh = 1;
constexpr uint16_t kPlatformIso = 2;
constexpr uint16_t kPlatformWindows = 3;

// Mac platform, encoding 0 (Roman): old fonts store text in the Roman variant
// that suits the record's language. The numbers are Macintosh language codes.
CFStringEncoding macRomanVariantForLanguage(uint16_t languageId) {
  switch (languageId) {
    case 15:  // Icelandic
      return kCFStringEncodingMacIcelandic;
    case 17:  // Turkish
      return kCFStringEncodingMacTurkish;
    case 18:  // Croatian
      return kCFStringEncodingMacCroatian;
    case 37:  // Romanian
      return kCFStringEncodingMacRomanian;
    case 24:  // Lithuanian
    case 25:  // Polish
    case 26:  // Hungarian
    case 27:  // Estonian
    case 28:  // Latvian
    case 36:  // Albanian
    case 38:  // Czech
    case 39:  // Slovak
    case 40:  // Slovenian
      return kCFStringEncodingMacCentralEurRoman;
    default:
      return kCFStringEncodingMacRoman;
  }
}

// Mac platform, other encodings: the encoding ID is the Mac script code, which
// CoreFoundation uses as the value of its kCFStringEncodingMac* constants.
// Script 8 (right-to-left symbols) and 32 (uninterpreted) have no text
// encoding. fontTools additionally reads IDs 35 and 37 as Turkish and
// Icelandic, which are CoreFoundation's own numbers for those variants.
std::optional<CFStringEncoding> macEncoding(uint16_t encodingId, uint16_t languageId) {
  if (encodingId == 0) return macRomanVariantForLanguage(languageId);
  if ((encodingId >= 1 && encodingId <= 31 && encodingId != 8) || encodingId == 35 || encodingId == 37)
    return static_cast<CFStringEncoding>(encodingId);
  return std::nullopt;
}

// Windows platform, encodings 2-6. The Windows code pages are supersets of the
// plain standards fontTools names (Shift-JIS, GB 2312, Big5, EUC-KR), so
// characters that only the Windows variants define decode too.
std::optional<CFStringEncoding> windowsMultiByteEncoding(uint16_t encodingId) {
  switch (encodingId) {
    case 2:
      return kCFStringEncodingDOSJapanese;  // code page 932
    case 3:
      return kCFStringEncodingDOSChineseSimplif;  // code page 936
    case 4:
      return kCFStringEncodingDOSChineseTrad;  // code page 950
    case 5:
      return kCFStringEncodingDOSKorean;  // code page 949
    case 6:
      return kCFStringEncodingWindowsKoreanJohab;
    default:
      return std::nullopt;
  }
}

std::optional<CFStringEncoding> encodingFor(uint16_t platformId, uint16_t encodingId, uint16_t languageId) {
  switch (platformId) {
    case kPlatformUnicode:
      return kCFStringEncodingUTF16BE;
    case kPlatformMacintosh:
      return macEncoding(encodingId, languageId);
    case kPlatformIso:
      switch (encodingId) {
        case 0:
          return kCFStringEncodingASCII;
        case 1:
          return kCFStringEncodingUTF16BE;
        case 2:
          return kCFStringEncodingISOLatin1;
        default:
          return std::nullopt;
      }
    case kPlatformWindows:
      if (encodingId == 0 || encodingId == 1 || encodingId == 10) return kCFStringEncodingUTF16BE;
      return windowsMultiByteEncoding(encodingId);
    default:
      return std::nullopt;
  }
}

}  // namespace

std::optional<std::string> decodeLegacyName(uint16_t platformId, uint16_t encodingId, uint16_t languageId,
                                            std::span<const uint8_t> bytes) {
  const auto encoding = encodingFor(platformId, encodingId, languageId);
  if (!encoding || !CFStringIsEncodingAvailable(*encoding)) return std::nullopt;

  // Windows multi-byte names are stored as 16-bit units: a double-byte
  // character keeps its two bytes, a single-byte character gets a zero high
  // byte. No lead or trail byte of these encodings is zero, so removing every
  // zero byte restores the plain multi-byte stream.
  std::vector<uint8_t> multiByte;
  if (platformId == kPlatformWindows && windowsMultiByteEncoding(encodingId)) {
    std::copy_if(bytes.begin(), bytes.end(), std::back_inserter(multiByte), [](uint8_t b) { return b != 0; });
    bytes = multiByte;
  }

  Ref<CFStringRef> text(CFStringCreateWithBytes(kCFAllocatorDefault, bytes.data(), static_cast<CFIndex>(bytes.size()),
                                                *encoding, false));
  if (!text) return std::nullopt;
  return darwin::toUtf8(text.get());
}

}  // namespace sfi::platform
