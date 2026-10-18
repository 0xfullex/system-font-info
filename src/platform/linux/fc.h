#pragma once

#include <fontconfig/fontconfig.h>

#include <compare>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

// The oldest supported fontconfig is 2.13, whose headers lack these object names. They are plain strings: on a
// runtime that predates an object fontconfig registers the unknown name on first use and no pattern holds a value
// for it, so the platform field is absent rather than the build failing.
#ifndef FC_FONT_WRAPPER  // fontconfig 2.15
#define FC_FONT_WRAPPER "fontwrapper"
#endif
#ifndef FC_ORDER  // fontconfig 2.14
#define FC_ORDER "order"
#endif
#ifndef FC_FONT_HAS_HINT  // fontconfig 2.14
#define FC_FONT_HAS_HINT "fonthashint"
#endif

namespace sfi::platform::linux_backend {

template <typename T, auto Destroy>
struct Deleter {
  void operator()(T* value) const { Destroy(value); }
};
template <typename T, auto Destroy>
using Owned = std::unique_ptr<T, Deleter<T, Destroy>>;
using Config = Owned<FcConfig, FcConfigDestroy>;
using Pattern = Owned<FcPattern, FcPatternDestroy>;
using FontSet = Owned<FcFontSet, FcFontSetDestroy>;
using ObjectSet = Owned<FcObjectSet, FcObjectSetDestroy>;
using StrList = Owned<FcStrList, FcStrListDone>;

inline void require(bool result, const char* operation) {
  if (!result) throw std::runtime_error(std::string("system-font-info: ") + operation + " failed");
}
inline std::optional<std::string> string(FcPattern* pattern, const char* object, int index = 0) {
  FcChar8* value = nullptr;
  if (FcPatternGetString(pattern, object, index, &value) != FcResultMatch) return std::nullopt;
  return reinterpret_cast<const char*>(value);
}
inline std::optional<int> integer(FcPattern* pattern, const char* object) {
  int value = 0;
  if (FcPatternGetInteger(pattern, object, 0, &value) != FcResultMatch) return std::nullopt;
  return value;
}
inline std::optional<double> number(FcPattern* pattern, const char* object) {
  double value = 0;
  if (FcPatternGetDouble(pattern, object, 0, &value) != FcResultMatch) return std::nullopt;
  return value;
}
inline std::optional<bool> boolean(FcPattern* pattern, const char* object) {
  FcBool value = FcFalse;
  if (FcPatternGetBool(pattern, object, 0, &value) != FcResultMatch) return std::nullopt;
  return value != FcFalse;
}
inline FcCharSet* charset(FcPattern* pattern) {
  FcCharSet* value = nullptr;
  require(FcPatternGetCharSet(pattern, FC_CHARSET, 0, &value) == FcResultMatch, "reading FC_CHARSET");
  return value;
}

struct Identity {
  std::string path;
  uint32_t index;
  auto operator<=>(const Identity&) const = default;
};
inline Identity identity(FcPattern* pattern) {
  auto path = string(pattern, FC_FILE);
  auto index = integer(pattern, FC_INDEX);
  require(path.has_value() && index.has_value(), "reading font identity");
  return {*path, uint32_t(*index)};
}
inline Identity resource(Identity value) {
  value.index &= 0xffff;
  return value;
}

}  // namespace sfi::platform::linux_backend
