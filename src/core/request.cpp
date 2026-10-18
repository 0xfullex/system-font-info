#include "request.h"

#include <algorithm>
#include <stdexcept>

namespace sfi {
namespace {

constexpr std::string_view kOpenTypePrefix = "faces.opentype.";

const FieldInfo* findField(std::string_view name) {
  const auto found = std::find_if(kFields.begin(), kFields.end(), [&](const FieldInfo& info) { return info.name == name; });
  return found == kFields.end() ? nullptr : &*found;
}

const PlatformSource& sourceOf(const FieldInfo& info) { return info.platforms[size_t(kCurrentPlatform)]; }

}  // namespace

bool Request::wantsAnyFace() const {
  if (platformObject) return true;
  for (const FieldInfo& info : kFields) {
    if (info.level != Level::Family && wants(info.field)) return true;
  }
  return false;
}

bool Request::wantsFamily(std::string_view family) const {
  return !families || std::find(families->begin(), families->end(), family) != families->end();
}

bool Request::needs(Group group) const {
  if (group == Group::A) return true;
  const GroupMask bit = GroupMask(1) << size_t(group);
  for (const FieldInfo& info : kFields) {
    if (wants(info.field) && (sourceOf(info).always & bit)) return true;
  }
  return false;
}

bool Request::mayNeed(Group group) const {
  if (needs(group)) return true;
  const GroupMask bit = GroupMask(1) << size_t(group);
  for (const FieldInfo& info : kFields) {
    if (wants(info.field) && (sourceOf(info).conditional & bit)) return true;
  }
  return false;
}

Request makeRequest(const std::vector<std::string>& fieldNames, std::optional<std::vector<std::string>> families,
                    const std::vector<std::string>& orthographyNames) {
  Request request;
  for (const std::string& name : fieldNames) {
    const FieldInfo* info = findField(name);
    if (!info) throw std::invalid_argument("system-font-info: unknown field '" + name + "' in the native request");
    if (info->level == Level::Platform) request.platformObject = true;
    const PlatformSource& source = sourceOf(*info);
    if (source.derived) {
      throw std::invalid_argument("system-font-info: the native request names the derived field '" + name + "'");
    }
    if (!source.available) continue;
    request.fields.set(size_t(info->field));
    if (info->level == Level::OpenType) {
      const auto entry = sfnt::openTypeEntryFromName(info->name.substr(kOpenTypePrefix.size()));
      if (!entry) throw std::logic_error("system-font-info: the catalogue has the opentype field '" + name + "' that sfnt does not know");
      request.openTypeSelection.select(*entry);
    }
  }
  request.families = std::move(families);
  for (const std::string& name : orthographyNames) {
    const orthography::Orthography* orthography = orthography::find(name);
    if (!orthography) throw std::invalid_argument("system-font-info: unknown orthography '" + name + "' in the native request");
    request.orthographies.push_back(orthography);
  }
  return request;
}

}  // namespace sfi
