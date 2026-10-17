#pragma once

#include <cstdint>

// The character encodings in which a `name` table string may be stored besides
// UTF-16BE, ISO 8859-1 and ASCII. sfnt decides which one a record uses (see
// names.cpp) and a platform backend turns bytes of that encoding into text
// (platform::decodeLegacyText), so the rule is the same on every platform and
// a backend only converts.
//
// The rule is that of fontTools (ttLib/tables/_n_a_m_e.py and
// misc/encodingTools.py), the most widely used reader of old fonts, so both
// tools read them the same way. It selects exactly the encodings below:
//
//   - Macintosh platform, encoding ID 0 (Roman): the Roman variant that suits
//     the record's Macintosh language code, else plain Mac Roman.
//   - Macintosh platform, encoding IDs 1-31 except 8: the Mac script of that
//     number (the script manager's script codes). Script 29 is the Central
//     European Roman set, the same as the language variant of ID 0. Script 8
//     (right-to-left symbols) and 32 (uninterpreted) have no text encoding.
//   - Macintosh platform, encoding IDs 35 and 37: read as Mac Turkish and Mac
//     Icelandic, as fontTools does.
//   - Windows platform, encoding IDs 2-6: the East Asian Windows code pages.
//     They are supersets of the plain standards fontTools names (Shift-JIS,
//     GB 2312, Big5, EUC-KR), so characters only the Windows variants define
//     decode too.

namespace sfi::sfnt {

enum class LegacyEncoding : uint8_t {
  // Mac Roman and the variants that old fonts store for particular languages.
  MacRoman,
  MacIcelandic,
  MacTurkish,
  MacCroatian,
  MacRomanian,
  MacCentralEuropean,

  // Mac scripts, named after the script of the same script code.
  MacJapanese,
  MacChineseTraditional,
  MacKorean,
  MacArabic,
  MacHebrew,
  MacGreek,
  MacCyrillic,
  MacDevanagari,
  MacGurmukhi,
  MacGujarati,
  MacOriya,
  MacBengali,
  MacTamil,
  MacTelugu,
  MacKannada,
  MacMalayalam,
  MacSinhalese,
  MacBurmese,
  MacKhmer,
  MacThai,
  MacLao,
  MacGeorgian,
  MacArmenian,
  MacChineseSimplified,
  MacTibetan,
  MacMongolian,
  MacEthiopic,
  MacVietnamese,
  MacExtendedArabic,

  // Windows code pages, named by their number.
  Windows932,   // Japanese (Shift-JIS)
  Windows936,   // Simplified Chinese (GBK)
  Windows950,   // Traditional Chinese (Big5)
  Windows949,   // Korean (Unified Hangul Code)
  Windows1361,  // Korean (Johab)
};

}  // namespace sfi::sfnt
