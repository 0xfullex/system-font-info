#include <stdexcept>

#include "../../core/platform.h"
#include "dwrite.h"

namespace sfi::platform {
namespace {

UINT codePage(sfnt::LegacyEncoding encoding) {
  using sfnt::LegacyEncoding;
  // Exact Windows code-page counterparts only. A same-language Windows/ISCII page is not a Mac converter.
  switch (encoding) {
    case LegacyEncoding::MacRoman:
      return 10000;
    case LegacyEncoding::MacIcelandic:
      return 10079;
    case LegacyEncoding::MacTurkish:
      return 10081;
    case LegacyEncoding::MacCroatian:
      return 10082;
    case LegacyEncoding::MacRomanian:
      return 10010;
    case LegacyEncoding::MacCentralEuropean:
      return 10029;
    case LegacyEncoding::MacJapanese:
      return 10001;
    case LegacyEncoding::MacChineseTraditional:
      return 10002;
    case LegacyEncoding::MacKorean:
      return 10003;
    case LegacyEncoding::MacArabic:
      return 10004;
    case LegacyEncoding::MacHebrew:
      return 10005;
    case LegacyEncoding::MacGreek:
      return 10006;
    case LegacyEncoding::MacCyrillic:
      return 10007;
    case LegacyEncoding::MacChineseSimplified:
      return 10008;
    case LegacyEncoding::MacThai:
      return 10021;
    case LegacyEncoding::Windows932:
      return 932;
    case LegacyEncoding::Windows936:
      return 936;
    case LegacyEncoding::Windows950:
      return 950;
    case LegacyEncoding::Windows949:
      return 949;
    case LegacyEncoding::Windows1361:
      return 1361;
    case LegacyEncoding::MacDevanagari:
    case LegacyEncoding::MacGurmukhi:
    case LegacyEncoding::MacGujarati:
    case LegacyEncoding::MacOriya:
    case LegacyEncoding::MacBengali:
    case LegacyEncoding::MacTamil:
    case LegacyEncoding::MacTelugu:
    case LegacyEncoding::MacKannada:
    case LegacyEncoding::MacMalayalam:
    case LegacyEncoding::MacSinhalese:
    case LegacyEncoding::MacBurmese:
    case LegacyEncoding::MacKhmer:
    case LegacyEncoding::MacLao:
    case LegacyEncoding::MacGeorgian:
    case LegacyEncoding::MacArmenian:
    case LegacyEncoding::MacTibetan:
    case LegacyEncoding::MacMongolian:
    case LegacyEncoding::MacEthiopic:
    case LegacyEncoding::MacVietnamese:
    case LegacyEncoding::MacExtendedArabic:
      return 0;
  }
  throw std::runtime_error("system-font-info: unmapped legacy encoding");
}

}  // namespace

std::optional<std::string> decodeLegacyText(sfnt::LegacyEncoding encoding, std::span<const uint8_t> bytes) {
  if (bytes.empty()) return std::string{};
  const UINT page = codePage(encoding);
  if (!page || !IsValidCodePage(page)) return std::nullopt;
  const auto* input = reinterpret_cast<const char*>(bytes.data());
  const int length = static_cast<int>(bytes.size());
  const int size = MultiByteToWideChar(page, MB_ERR_INVALID_CHARS, input, length, nullptr, 0);
  if (!size) return std::nullopt;
  std::wstring text(size, L'\0');
  if (!MultiByteToWideChar(page, MB_ERR_INVALID_CHARS, input, length, text.data(), size)) return std::nullopt;
  return win32::toUtf8(text);
}

}  // namespace sfi::platform
