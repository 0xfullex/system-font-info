#include "dwrite.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace sfi::platform::win32 {

void check(HRESULT result, const char* operation) {
  if (SUCCEEDED(result)) return;
  wchar_t* message = nullptr;
  const DWORD size =
      FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                     nullptr, static_cast<DWORD>(result), 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
  std::ostringstream error;
  error << "system-font-info: " << operation << " (0x" << std::hex << static_cast<uint32_t>(result) << ")";
  if (size) {
    const std::wstring text(message, size);
    LocalFree(message);
    error << ": " << toUtf8(text);
  }
  throw std::runtime_error(error.str());
}

void checkRequiredInterface(HRESULT result, const char* operation) {
  // Older DirectWrite builds answer E_NOINTERFACE for an interface they lack; saying only that would hide the cause.
  if (result == E_NOINTERFACE)
    throw std::runtime_error("system-font-info requires Windows 10 version 1809 (build 17763) or later");
  check(result, operation);
}

std::string toUtf8(std::wstring_view text) {
  if (text.empty()) return {};
  // Flag 0 replaces a lone surrogate with U+FFFD, as the name-table reader does. WC_ERR_INVALID_CHARS would fail
  // the whole enumeration for one badly encoded font name.
  const int size =
      WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
  if (!size) throw std::runtime_error("system-font-info: UTF-8 conversion failed");
  std::string result(size, '\0');
  if (!WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr,
                           nullptr))
    throw std::runtime_error("system-font-info: UTF-8 conversion failed");
  return result;
}

std::wstring toWide(std::string_view text) {
  if (text.empty()) return {};
  const int size =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
  if (!size) throw std::runtime_error("system-font-info: invalid UTF-8 text");
  std::wstring result(size, L'\0');
  if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(),
                           size))
    throw std::runtime_error("system-font-info: UTF-16 conversion failed");
  return result;
}

ComPtr<IDWriteFactory6> createFactory() {
  ComPtr<IDWriteFactory6> factory;
  checkRequiredInterface(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory6),
                                             reinterpret_cast<IUnknown**>(factory.GetAddressOf())),
                         "DWriteCreateFactory");
  return factory;
}

LocalizedStrings localizedStrings(IDWriteLocalizedStrings* strings) {
  LocalizedStrings result;
  if (!strings) return result;
  for (UINT32 i = 0; i < strings->GetCount(); ++i) {
    UINT32 size = 0;
    check(strings->GetLocaleNameLength(i, &size), "GetLocaleNameLength");
    std::wstring locale(size + 1, L'\0');
    check(strings->GetLocaleName(i, locale.data(), size + 1), "GetLocaleName");
    locale.resize(size);
    check(strings->GetStringLength(i, &size), "GetStringLength");
    std::wstring value(size + 1, L'\0');
    check(strings->GetString(i, value.data(), size + 1), "GetString");
    value.resize(size);
    result.push_back({toUtf8(locale), toUtf8(value)});
  }
  return result;
}

LocalizedStrings informationalStrings(IDWriteFont* font, DWRITE_INFORMATIONAL_STRING_ID id) {
  ComPtr<IDWriteLocalizedStrings> strings;
  BOOL exists = FALSE;
  check(font->GetInformationalStrings(id, &strings, &exists), "GetInformationalStrings");
  return exists ? localizedStrings(strings.Get()) : LocalizedStrings{};
}

std::string preferredString(const LocalizedStrings& strings) {
  for (const auto& item : strings) {
    if (item.language == "en-us" || item.language == "en-US") return item.value;
  }
  return strings.empty() ? std::string{} : strings.front().value;
}

Value localizedValue(const LocalizedStrings& strings) {
  Value result = Value::array();
  for (const auto& item : strings) result.push(Value::object().set("language", item.language).set("value", item.value));
  return result;
}

std::string axisTag(DWRITE_FONT_AXIS_TAG tag) {
  const uint32_t value = static_cast<uint32_t>(tag);
  std::string result;
  for (size_t i = 0; i < 4; ++i) result.push_back(static_cast<char>((value >> (i * 8)) & 0xff));
  return result;
}

std::vector<DWRITE_FONT_AXIS_VALUE> axisValues(IDWriteFontFaceReference* reference) {
  ComPtr<IDWriteFontFaceReference1> extended;
  check(reference->QueryInterface(IID_PPV_ARGS(&extended)), "IDWriteFontFaceReference1");
  const UINT32 count = extended->GetFontAxisValueCount();
  std::vector<DWRITE_FONT_AXIS_VALUE> values(count);
  check(extended->GetFontAxisValues(values.data(), count), "GetFontAxisValues");
  return values;
}

Reference describeReference(IDWriteFontFaceReference* reference) {
  Reference result;
  result.reference = reference;
  check(reference->GetFontFile(&result.file), "GetFontFile");
  ComPtr<IDWriteFontFileLoader> loader;
  check(result.file->GetLoader(&loader), "GetLoader");
  const void* key = nullptr;
  UINT32 keySize = 0;
  check(result.file->GetReferenceKey(&key, &keySize), "GetReferenceKey");
  ComPtr<IDWriteLocalFontFileLoader> local;
  if (SUCCEEDED(loader.As(&local))) {
    UINT32 length = 0;
    check(local->GetFilePathLengthFromKey(key, keySize, &length), "GetFilePathLengthFromKey");
    std::wstring path(length + 1, L'\0');
    check(local->GetFilePathFromKey(key, keySize, path.data(), length + 1), "GetFilePathFromKey");
    path.resize(length);
    result.path = toUtf8(path);
    result.key.first = result.path;
  } else {
    // A reference key has meaning only to its loader. Keep this internal identity separate from local paths.
    result.key.first = std::string(1, '\0') + std::to_string(reinterpret_cast<uintptr_t>(loader.Get())) + ":" +
                       std::string(static_cast<const char*>(key), keySize);
  }
  result.key.second = reference->GetFontFaceIndex();
  return result;
}

Reference fontReference(IDWriteFont* font) {
  ComPtr<IDWriteFont3> font3;
  check(font->QueryInterface(IID_PPV_ARGS(&font3)), "IDWriteFont3");
  ComPtr<IDWriteFontFaceReference> reference;
  check(font3->GetFontFaceReference(&reference), "GetFontFaceReference");
  return describeReference(reference.Get());
}

std::set<std::string> inactiveFamilies() {
  HKEY rawKey = nullptr;
  const LSTATUS status = RegOpenKeyExW(
      HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Font Management\\Inactive Fonts", 0,
      KEY_QUERY_VALUE, &rawKey);
  if (status == ERROR_FILE_NOT_FOUND) return {};
  check(HRESULT_FROM_WIN32(status), "RegOpenKeyExW(Inactive Fonts)");
  struct Key {
    HKEY value;
    ~Key() { RegCloseKey(value); }
  } key{rawKey};
  std::set<std::string> result;
  std::vector<wchar_t> name(16384);
  for (DWORD index = 0;; ++index) {
    DWORD length = static_cast<DWORD>(name.size());
    const LSTATUS enumerated =
        RegEnumValueW(key.value, index, name.data(), &length, nullptr, nullptr, nullptr, nullptr);
    if (enumerated == ERROR_NO_MORE_ITEMS) break;
    check(HRESULT_FROM_WIN32(enumerated), "RegEnumValueW(Inactive Fonts)");
    result.insert(toUtf8(std::wstring_view(name.data(), length)));
  }
  return result;
}

std::vector<std::pair<uint32_t, uint32_t>> coverageOf(IDWriteFont* font) {
  ComPtr<IDWriteFont1> font1;
  check(font->QueryInterface(IID_PPV_ARGS(&font1)), "IDWriteFont1");
  UINT32 count = 0;
  const HRESULT status = font1->GetUnicodeRanges(0, nullptr, &count);
  if (status != E_NOT_SUFFICIENT_BUFFER) check(status, "GetUnicodeRanges(count)");
  std::vector<DWRITE_UNICODE_RANGE> ranges(count);
  check(font1->GetUnicodeRanges(count, ranges.data(), &count), "GetUnicodeRanges");
  std::vector<std::pair<uint32_t, uint32_t>> result;
  for (const auto& range : ranges) {
    if (!result.empty() && range.first <= result.back().second + 1) {
      result.back().second = std::max(result.back().second, range.last);
    } else {
      result.emplace_back(range.first, range.last);
    }
  }
  return result;
}

bool covers(const std::vector<std::pair<uint32_t, uint32_t>>& ranges, uint32_t codePoint) {
  const auto after = std::upper_bound(ranges.begin(), ranges.end(), codePoint,
                                      [](uint32_t point, const auto& range) { return point < range.first; });
  return after != ranges.begin() && codePoint <= std::prev(after)->second;
}

}  // namespace sfi::platform::win32
