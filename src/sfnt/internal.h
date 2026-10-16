#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "sfnt.h"

// Helpers shared by the .cpp files of this directory; not part of the public
// API (sfnt.h).

namespace sfi::sfnt {

// ---- text (text.cpp) --------------------------------------------------------

// UTF-16BE -> UTF-8. A lone surrogate, and a trailing odd byte, become U+FFFD.
std::string utf16beToUtf8(std::span<const uint8_t> bytes);
// Each byte is the code point of the same value (ISO 8859-1 -> UTF-8).
std::string latin1ToUtf8(std::span<const uint8_t> bytes);
// Lowercase hex, two digits per byte.
std::string toHex(std::span<const uint8_t> bytes);

// ---- table values (the pieces of the `opentype` Value) -----------------------

// [{ language, value }, ...]
Value localizedStringsToValue(const LocalizedStrings& strings);
// The `names` array: one object per record.
Value nameRecordsToValue(const std::vector<NameRecord>& records);

Value fvarToValue(const Fvar& fvar, const std::vector<NameRecord>& names);

// A GSUB or GPOS table: { scripts, features, featureNames? }.
Value layoutToValue(std::span<const uint8_t> table, const std::vector<NameRecord>& names);

Value headToValue(std::span<const uint8_t> table);
// hhea, and vhea with the same field layout.
Value horizontalHeaderToValue(std::span<const uint8_t> table);
Value os2ToValue(std::span<const uint8_t> table);
Value postToValue(std::span<const uint8_t> table);
Value maxpToValue(std::span<const uint8_t> table);
// Sets `hasSymbolSubtable` when the cmap has a (3,0) subtable.
Value cmapToValue(std::span<const uint8_t> table, bool& hasSymbolSubtable);
Value metaToValue(std::span<const uint8_t> table);

}  // namespace sfi::sfnt
