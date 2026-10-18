#include <set>

#include "../../core/platform.h"
#include "collection.h"
#include "face_reader.h"

namespace sfi::platform {

std::vector<Family> enumerateFonts(const Request& request, WorkLog& work) {
  if (request.families && request.families->empty()) return {};
  win32::Collection collection(request);
  auto* fonts = collection.fonts();
  std::vector<UINT32> candidates;
  if (request.families) {
    std::set<UINT32> seen;
    for (const auto& name : *request.families) {
      UINT32 index = 0;
      BOOL exists = FALSE;
      win32::check(fonts->FindFamilyName(win32::toWide(name).c_str(), &index, &exists), "FindFamilyName");
      if (exists && seen.insert(index).second) candidates.push_back(index);
    }
  } else {
    for (UINT32 index = 0; index < fonts->GetFontFamilyCount(); ++index) candidates.push_back(index);
  }

  const auto inactive =
      request.wants(Field::FacesPlatformInactiveFamilyPreference) ? win32::inactiveFamilies() : std::set<std::string>{};
  win32::FaceReader reader(request, work, collection);
  std::vector<Family> result;
  for (UINT32 index : candidates) {
    win32::ComPtr<IDWriteFontFamily> nativeFamily;
    win32::check(fonts->GetFontFamily(index, &nativeFamily), "GetFontFamily");
    win32::ComPtr<IDWriteLocalizedStrings> nativeNames;
    win32::check(nativeFamily->GetFamilyNames(&nativeNames), "GetFamilyNames");
    const auto names = win32::localizedStrings(nativeNames.Get());
    Family family;
    family.family = win32::preferredString(names);
    // FindFamilyName accepts case-insensitive and localized aliases. This is the only public family filter.
    if (!request.wantsFamily(family.family)) continue;
    if (request.wants(Field::FamilyNames)) family.familyNames = names;

    if (request.wantsAnyFace()) {
      bool hidden = false;
      for (const auto& name : names) hidden = hidden || inactive.contains(name.value);
      std::set<std::string> identities;
      for (UINT32 member = 0; member < nativeFamily->GetFontCount(); ++member) {
        win32::ComPtr<IDWriteFont> font;
        win32::check(nativeFamily->GetFont(member, &font), "GetFont");
        auto read = reader.read(font.Get(), names, hidden);
        // A repeated identity is the same face listed again: the first member in order stays, even if other fields
        // of the later one differ.
        if (identities.insert(read.identity).second) family.faces.push_back(std::move(read.face));
      }
    }
    if (!request.orthographies.empty()) {
      win32::ComPtr<IDWriteFont> matched;
      win32::check(nativeFamily->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
                                                      DWRITE_FONT_STYLE_NORMAL, &matched),
                   "GetFirstMatchingFont");
      const auto ranges = win32::coverageOf(matched.Get());
      ++work.characterSets;
      for (const auto* orthography : request.orthographies)
        family.languageCoverage.push_back(
            orthography::countCovered(*orthography, [&](uint32_t point) { return win32::covers(ranges, point); }));
    }
    result.push_back(std::move(family));
  }
  return result;
}

}  // namespace sfi::platform
