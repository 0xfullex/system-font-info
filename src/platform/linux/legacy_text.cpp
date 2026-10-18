#include <iconv.h>

#include "../../core/platform.h"

namespace sfi::platform {
namespace {
const char* encodingName(sfnt::LegacyEncoding encoding) {
  using E = sfnt::LegacyEncoding;
  switch (encoding) {
    case E::MacRoman:
      return "MACINTOSH";
    case E::MacIcelandic:
      return "MAC-IS";
    case E::MacCentralEuropean:
      return "MAC-CENTRALEUROPE";
    case E::MacCyrillic:
      return "MAC-CYRILLIC";
    case E::Windows932:
      return "CP932";
    case E::Windows936:
      return "CP936";
    case E::Windows950:
      return "CP950";
    case E::Windows949:
      return "CP949";
    case E::Windows1361:
      return "JOHAB";
    case E::MacTurkish:
    case E::MacCroatian:
    case E::MacRomanian:
    case E::MacJapanese:
    case E::MacChineseTraditional:
    case E::MacKorean:
    case E::MacArabic:
    case E::MacHebrew:
    case E::MacGreek:
    case E::MacDevanagari:
    case E::MacGurmukhi:
    case E::MacGujarati:
    case E::MacOriya:
    case E::MacBengali:
    case E::MacTamil:
    case E::MacTelugu:
    case E::MacKannada:
    case E::MacMalayalam:
    case E::MacSinhalese:
    case E::MacBurmese:
    case E::MacKhmer:
    case E::MacThai:
    case E::MacLao:
    case E::MacGeorgian:
    case E::MacArmenian:
    case E::MacChineseSimplified:
    case E::MacTibetan:
    case E::MacMongolian:
    case E::MacEthiopic:
    case E::MacVietnamese:
    case E::MacExtendedArabic:
      return nullptr;
  }
  return nullptr;
}
}  // namespace

std::optional<std::string> decodeLegacyText(sfnt::LegacyEncoding encoding, std::span<const uint8_t> bytes) {
  const char* name = encodingName(encoding);
  if (!name) return std::nullopt;
  iconv_t converter = iconv_open("UTF-8", name);
  if (converter == (iconv_t)-1) return std::nullopt;
  std::string result(bytes.size() * 4 + 16, '\0');
  char* input = reinterpret_cast<char*>(const_cast<uint8_t*>(bytes.data()));
  size_t remaining = bytes.size();
  char* output = result.data();
  size_t available = result.size();
  const bool success = iconv(converter, &input, &remaining, &output, &available) != size_t(-1);
  iconv_close(converter);
  if (!success) return std::nullopt;
  result.resize(result.size() - available);
  return result;
}
}  // namespace sfi::platform
