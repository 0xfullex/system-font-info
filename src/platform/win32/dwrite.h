#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwrite_3.h>
#include <windows.h>
#include <wrl/client.h>

#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "../../core/model.h"

namespace sfi::platform::win32 {

using Microsoft::WRL::ComPtr;
using ResourceKey = std::pair<std::string, uint32_t>;

void check(HRESULT result, const char* operation);
// Like `check`, for a query of an interface that only recent Windows builds have: E_NOINTERFACE becomes the error
// naming the minimum supported Windows version.
void checkRequiredInterface(HRESULT result, const char* operation);
std::string toUtf8(std::wstring_view text);
std::wstring toWide(std::string_view text);
ComPtr<IDWriteFactory6> createFactory();
LocalizedStrings localizedStrings(IDWriteLocalizedStrings* strings);
LocalizedStrings informationalStrings(IDWriteFont* font, DWRITE_INFORMATIONAL_STRING_ID id);
std::string preferredString(const LocalizedStrings& strings);
Value localizedValue(const LocalizedStrings& strings);
std::string axisTag(DWRITE_FONT_AXIS_TAG tag);
std::vector<DWRITE_FONT_AXIS_VALUE> axisValues(IDWriteFontFaceReference* reference);

struct Reference {
  ComPtr<IDWriteFontFaceReference> reference;
  ComPtr<IDWriteFontFile> file;
  std::string path;
  ResourceKey key;
};
Reference fontReference(IDWriteFont* font);
Reference describeReference(IDWriteFontFaceReference* reference);

std::set<std::string> inactiveFamilies();
std::vector<std::pair<uint32_t, uint32_t>> coverageOf(IDWriteFont* font);
bool covers(const std::vector<std::pair<uint32_t, uint32_t>>& ranges, uint32_t codePoint);

}  // namespace sfi::platform::win32
