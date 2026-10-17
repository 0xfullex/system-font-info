#include "../../core/platform.h"
#include "cf.h"

// decodeLegacyText: bytes in a legacy character encoding, converted with
// CoreFoundation's string encodings. Which encoding a `name` record uses is
// decided by sfnt; this file only names the CoreFoundation encoding of each.

namespace sfi::platform {
namespace {

using sfnt::LegacyEncoding;

CFStringEncoding cfEncoding(LegacyEncoding encoding) {
  switch (encoding) {
    case LegacyEncoding::MacRoman:
      return kCFStringEncodingMacRoman;
    case LegacyEncoding::MacIcelandic:
      return kCFStringEncodingMacIcelandic;
    case LegacyEncoding::MacTurkish:
      return kCFStringEncodingMacTurkish;
    case LegacyEncoding::MacCroatian:
      return kCFStringEncodingMacCroatian;
    case LegacyEncoding::MacRomanian:
      return kCFStringEncodingMacRomanian;
    case LegacyEncoding::MacCentralEuropean:
      return kCFStringEncodingMacCentralEurRoman;
    case LegacyEncoding::MacJapanese:
      return kCFStringEncodingMacJapanese;
    case LegacyEncoding::MacChineseTraditional:
      return kCFStringEncodingMacChineseTrad;
    case LegacyEncoding::MacKorean:
      return kCFStringEncodingMacKorean;
    case LegacyEncoding::MacArabic:
      return kCFStringEncodingMacArabic;
    case LegacyEncoding::MacHebrew:
      return kCFStringEncodingMacHebrew;
    case LegacyEncoding::MacGreek:
      return kCFStringEncodingMacGreek;
    case LegacyEncoding::MacCyrillic:
      return kCFStringEncodingMacCyrillic;
    case LegacyEncoding::MacDevanagari:
      return kCFStringEncodingMacDevanagari;
    case LegacyEncoding::MacGurmukhi:
      return kCFStringEncodingMacGurmukhi;
    case LegacyEncoding::MacGujarati:
      return kCFStringEncodingMacGujarati;
    case LegacyEncoding::MacOriya:
      return kCFStringEncodingMacOriya;
    case LegacyEncoding::MacBengali:
      return kCFStringEncodingMacBengali;
    case LegacyEncoding::MacTamil:
      return kCFStringEncodingMacTamil;
    case LegacyEncoding::MacTelugu:
      return kCFStringEncodingMacTelugu;
    case LegacyEncoding::MacKannada:
      return kCFStringEncodingMacKannada;
    case LegacyEncoding::MacMalayalam:
      return kCFStringEncodingMacMalayalam;
    case LegacyEncoding::MacSinhalese:
      return kCFStringEncodingMacSinhalese;
    case LegacyEncoding::MacBurmese:
      return kCFStringEncodingMacBurmese;
    case LegacyEncoding::MacKhmer:
      return kCFStringEncodingMacKhmer;
    case LegacyEncoding::MacThai:
      return kCFStringEncodingMacThai;
    case LegacyEncoding::MacLao:
      return kCFStringEncodingMacLaotian;
    case LegacyEncoding::MacGeorgian:
      return kCFStringEncodingMacGeorgian;
    case LegacyEncoding::MacArmenian:
      return kCFStringEncodingMacArmenian;
    case LegacyEncoding::MacChineseSimplified:
      return kCFStringEncodingMacChineseSimp;
    case LegacyEncoding::MacTibetan:
      return kCFStringEncodingMacTibetan;
    case LegacyEncoding::MacMongolian:
      return kCFStringEncodingMacMongolian;
    case LegacyEncoding::MacEthiopic:
      return kCFStringEncodingMacEthiopic;
    case LegacyEncoding::MacVietnamese:
      return kCFStringEncodingMacVietnamese;
    case LegacyEncoding::MacExtendedArabic:
      return kCFStringEncodingMacExtArabic;
    case LegacyEncoding::Windows932:
      return kCFStringEncodingDOSJapanese;
    case LegacyEncoding::Windows936:
      return kCFStringEncodingDOSChineseSimplif;
    case LegacyEncoding::Windows950:
      return kCFStringEncodingDOSChineseTrad;
    case LegacyEncoding::Windows949:
      return kCFStringEncodingDOSKorean;
    case LegacyEncoding::Windows1361:
      return kCFStringEncodingWindowsKoreanJohab;
  }
  __builtin_unreachable();
}

}  // namespace

std::optional<std::string> decodeLegacyText(sfnt::LegacyEncoding encoding, std::span<const uint8_t> bytes) {
  // Johab, for one, is not available in CoreFoundation.
  const CFStringEncoding cf = cfEncoding(encoding);
  if (!CFStringIsEncodingAvailable(cf)) return std::nullopt;

  darwin::Ref<CFStringRef> text(
      CFStringCreateWithBytes(kCFAllocatorDefault, bytes.data(), static_cast<CFIndex>(bytes.size()), cf, false));
  if (!text) return std::nullopt;
  return darwin::toUtf8(text.get());
}

}  // namespace sfi::platform
