#include "collection.h"

#include <stdexcept>

#include "testing.h"

namespace sfi::platform::win32 {

Collection::Collection(const Request& request) : request_(request), factory_(createFactory()) {
  // This overload is the WSS collection and explicitly disables the update check. Factory6's overload has no
  // checkForUpdates parameter: its first BOOL controls downloadable fonts instead.
  ComPtr<IDWriteFontCollection1> system;
  check(static_cast<IDWriteFactory3*>(factory_.Get())->GetSystemFontCollection(FALSE, &system, FALSE),
        "GetSystemFontCollection");
  check(system.As(&collection_), "IDWriteFontCollection");

  const auto registered = registeredFonts();
  if (registered.empty()) return;
  check(factory_->GetSystemFontSet(FALSE, &systemSet_), "GetSystemFontSet");
  ComPtr<IDWriteFontSetBuilder2> builder;
  check(factory_->CreateFontSetBuilder(&builder), "CreateFontSetBuilder");
  check(builder->AddFontSet(systemSet_.Get()), "AddFontSet");
  for (const auto& path : registered) {
    check(builder->AddFontFile(path.c_str()), "AddFontFile");
    processPaths_.insert(path);
  }
  ComPtr<IDWriteFontSet> combined;
  check(builder->CreateFontSet(&combined), "CreateFontSet");
  check(combined.As(&memberSet_), "IDWriteFontSet1");
  ComPtr<IDWriteFontCollection2> custom;
  check(
      factory_->CreateFontCollectionFromFontSet(combined.Get(), DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE, &custom),
      "CreateFontCollectionFromFontSet");
  check(custom.As(&collection_), "IDWriteFontCollection");
}

bool Collection::processFont(const std::string& path) const {
  const auto nativePath = toWide(path);
  for (const auto& registered : processPaths_) {
    if (CompareStringOrdinal(nativePath.data(), static_cast<int>(nativePath.size()), registered.data(),
                             static_cast<int>(registered.size()), TRUE) == CSTR_EQUAL)
      return true;
  }
  return false;
}

void Collection::loadResources() {
  if (resourcesLoaded_) return;
  resourcesLoaded_ = true;
  if (!systemSet_) check(factory_->GetSystemFontSet(FALSE, &systemSet_), "GetSystemFontSet");
  if (!memberSet_) memberSet_ = systemSet_;
  for (UINT32 index = 0; index < memberSet_->GetFontCount(); ++index) {
    ComPtr<IDWriteFontFaceReference1> reference;
    check(memberSet_->GetFontFaceReference(index, &reference), "GetFontFaceReference(set)");
    const auto described = describeReference(reference.Get());
    UINT32 count = 0;
    const HRESULT status = memberSet_->GetFontAxisRanges(index, nullptr, 0, &count);
    if (status != E_NOT_SUFFICIENT_BUFFER) check(status, "GetFontAxisRanges(count)");
    std::vector<DWRITE_FONT_AXIS_RANGE> ranges(count);
    check(memberSet_->GetFontAxisRanges(index, ranges.data(), count, &count), "GetFontAxisRanges");
    bool variable = false;
    for (const auto& range : ranges) variable = variable || range.minValue != range.maxValue;
    const auto [found, inserted] = variable_.emplace(described.key, variable);
    if (!inserted && found->second != variable)
      throw std::runtime_error("system-font-info: conflicting variable classification for " + described.path);
  }
}

bool Collection::variable(const ResourceKey& key) {
  loadResources();
  const auto found = variable_.find(key);
  if (found == variable_.end()) throw std::runtime_error("system-font-info: WSS member has no font-set resource");
  return found->second;
}

void Collection::loadSources() {
  if (sourcesLoaded_) return;
  sourcesLoaded_ = true;
  if (!systemSet_) check(factory_->GetSystemFontSet(FALSE, &systemSet_), "GetSystemFontSet");
  ComPtr<IDWriteFontSet3> set3;
  checkRequiredInterface(systemSet_.As(&set3), "IDWriteFontSet3");
  for (UINT32 index = 0; index < systemSet_->GetFontCount(); ++index) {
    ComPtr<IDWriteFontFaceReference1> reference;
    check(systemSet_->GetFontFaceReference(index, &reference), "GetFontFaceReference(source)");
    const auto described = describeReference(reference.Get());
    Source source;
    source.type = static_cast<uint32_t>(set3->GetFontSourceType(index));
    const UINT32 length = set3->GetFontSourceNameLength(index);
    if (length) {
      std::wstring name(length + 1, L'\0');
      check(set3->GetFontSourceName(index, name.data(), length + 1), "GetFontSourceName");
      name.resize(length);
      source.name = toUtf8(name);
    }
    const auto [found, inserted] = sources_.emplace(described.key, source);
    if (!inserted && (found->second.type != source.type || found->second.name != source.name))
      throw std::runtime_error("system-font-info: conflicting source metadata for " + described.path);
  }
}

Source Collection::source(const ResourceKey& key) {
  loadSources();
  const auto found = sources_.find(key);
  return found == sources_.end() ? Source{} : found->second;
}

}  // namespace sfi::platform::win32
