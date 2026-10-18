#include "to_js.h"

#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <variant>

namespace sfi {
namespace {

const char* styleName(Style style) {
  switch (style) {
    case Style::Normal:
      return "normal";
    case Style::Italic:
      return "italic";
    case Style::Oblique:
      return "oblique";
  }
  return "normal";
}

const char* scopeName(Scope scope) {
  switch (scope) {
    case Scope::System:
      return "system";
    case Scope::User:
      return "user";
    case Scope::Process:
      return "process";
    case Scope::Unknown:
      return "unknown";
  }
  return "unknown";
}

const char* formatName(Format format) {
  switch (format) {
    case Format::TrueType:
      return "truetype";
    case Format::Cff:
      return "cff";
    case Format::Cff2:
      return "cff2";
    case Format::Type1:
      return "type1";
    case Format::Bitmap:
      return "bitmap";
    case Format::Unknown:
      return "unknown";
  }
  return "unknown";
}

// One conversion run. The memo tables hold persistent references because faces
// are converted inside per-family handle scopes, which would otherwise
// invalidate the shared objects created in an earlier family.
class Converter {
 public:
  // `request` is null for a subtree converted on its own (`sharedToJs`); only `families` and what it calls need it.
  explicit Converter(Napi::Env env, const Request* request = nullptr) : env_(env), request_(request) {
    if (!request_) return;
    for (const FieldInfo& info : kFields) {
      if (info.level == Level::Platform && request_->wants(info.field)) {
        platformKeys_.emplace(info.name.substr(kPlatformPrefix.size()));
      }
    }
  }

  Napi::Array families(const std::vector<Family>& families) {
    auto result = Napi::Array::New(env_, families.size());
    for (size_t i = 0; i < families.size(); ++i) {
      Napi::EscapableHandleScope scope(env_);
      result.Set(static_cast<uint32_t>(i), scope.Escape(family(families[i])));
    }
    return result;
  }

  Napi::Value shared(const Value::Shared& subtree) {
    if (!subtree) return env_.Null();
    auto found = sharedObjects_.find(subtree.get());
    if (found != sharedObjects_.end()) return found->second.Value();
    Napi::Value converted = value(*subtree);
    // Only objects (and arrays) have an identity worth preserving.
    if (converted.IsObject()) sharedObjects_.emplace(subtree.get(), Napi::Persistent(converted.As<Napi::Object>()));
    return converted;
  }

 private:
  Napi::Value value(const Value& v) {
    return std::visit(
        [&](const auto& x) -> Napi::Value {
          using T = std::decay_t<decltype(x)>;
          if constexpr (std::is_same_v<T, std::monostate>) {
            return env_.Null();
          } else if constexpr (std::is_same_v<T, bool>) {
            return Napi::Boolean::New(env_, x);
          } else if constexpr (std::is_same_v<T, double>) {
            return Napi::Number::New(env_, x);
          } else if constexpr (std::is_same_v<T, std::string>) {
            return Napi::String::New(env_, x);
          } else if constexpr (std::is_same_v<T, Value::Array>) {
            auto array = Napi::Array::New(env_, x.size());
            for (size_t i = 0; i < x.size(); ++i) array.Set(static_cast<uint32_t>(i), value(x[i]));
            return array;
          } else if constexpr (std::is_same_v<T, Value::Object>) {
            auto object = Napi::Object::New(env_);
            for (const auto& [key, member] : x) object.Set(key, value(member));
            return object;
          } else {
            static_assert(std::is_same_v<T, Value::Shared>);
            return shared(x);
          }
        },
        v.data());
  }

  Napi::Array localized(const LocalizedStrings& strings) {
    auto array = Napi::Array::New(env_, strings.size());
    for (size_t i = 0; i < strings.size(); ++i) {
      auto item = Napi::Object::New(env_);
      item.Set("language", Napi::String::New(env_, strings[i].language));
      item.Set("value", Napi::String::New(env_, strings[i].value));
      array.Set(static_cast<uint32_t>(i), item);
    }
    return array;
  }

  Napi::Array stringArray(const std::vector<std::string>& strings) {
    auto array = Napi::Array::New(env_, strings.size());
    for (size_t i = 0; i < strings.size(); ++i)
      array.Set(static_cast<uint32_t>(i), Napi::String::New(env_, strings[i]));
    return array;
  }

  Napi::Value axes(const SharedAxes& shared) {
    auto found = sharedAxes_.find(shared.get());
    if (found != sharedAxes_.end()) return found->second.Value();
    auto array = Napi::Array::New(env_, shared->size());
    for (size_t i = 0; i < shared->size(); ++i) {
      const VariationAxis& axis = (*shared)[i];
      auto item = Napi::Object::New(env_);
      item.Set("tag", Napi::String::New(env_, axis.tag));
      item.Set("names", localized(axis.names));
      item.Set("min", Napi::Number::New(env_, axis.min));
      item.Set("max", Napi::Number::New(env_, axis.max));
      item.Set("default", Napi::Number::New(env_, axis.defaultValue));
      item.Set("hidden", Napi::Boolean::New(env_, axis.hidden));
      array.Set(static_cast<uint32_t>(i), item);
    }
    sharedAxes_.emplace(shared.get(), Napi::Persistent<Napi::Object>(array));
    return array;
  }

  // `os` and the requested platform fields of the backend's `platform` value, in its order.
  Napi::Value platform(const Value& platformValue) {
    auto out = Napi::Object::New(env_);
    const auto* members = std::get_if<Value::Object>(&platformValue.data());
    if (!members) return out;
    for (const auto& [key, member] : *members) {
      if (key == "os" || platformKeys_.contains(key)) out.Set(key, value(member));
    }
    return out;
  }

  Napi::Object face(const Face& f) {
    const Request& request = *request_;
    auto out = Napi::Object::New(env_);
    if (request.wants(Field::FacesPostscriptName)) out.Set("postscriptName", Napi::String::New(env_, f.postscriptName));
    if (request.wants(Field::FacesFullNames)) out.Set("fullNames", localized(f.fullNames));
    if (request.wants(Field::FacesStyleNames)) out.Set("styleNames", localized(f.styleNames));
    if (request.wants(Field::FacesFamilyNames)) out.Set("familyNames", localized(f.familyNames));
    if (request.wants(Field::FacesWeight)) out.Set("weight", Napi::Number::New(env_, f.weight));
    if (request.wants(Field::FacesWidth)) out.Set("width", Napi::Number::New(env_, f.width));
    if (request.wants(Field::FacesStyle)) out.Set("style", Napi::String::New(env_, styleName(f.style)));
    if (request.wants(Field::FacesMonospace)) out.Set("monospace", Napi::Boolean::New(env_, f.monospace));
    if (request.wants(Field::FacesColor)) out.Set("color", Napi::Boolean::New(env_, f.color));
    if (request.wants(Field::FacesSymbol)) out.Set("symbol", Napi::Boolean::New(env_, f.symbol));
    if (request.wants(Field::FacesEnabled)) out.Set("enabled", Napi::Boolean::New(env_, f.enabled));

    if (request.wants(Field::FacesSimulations)) {
      auto simulations = Napi::Object::New(env_);
      simulations.Set("bold", Napi::Boolean::New(env_, f.simulatedBold));
      simulations.Set("oblique", Napi::Boolean::New(env_, f.simulatedOblique));
      out.Set("simulations", simulations);
    }

    if (request.wants(Field::FacesSupportedLanguages)) out.Set("supportedLanguages", stringArray(f.supportedLanguages));
    if (request.wants(Field::FacesScope)) out.Set("scope", Napi::String::New(env_, scopeName(f.scope)));
    if (request.wants(Field::FacesFormat)) out.Set("format", Napi::String::New(env_, formatName(f.format)));
    if (request.wants(Field::FacesFile) && f.file) {
      auto file = Napi::Object::New(env_);
      file.Set("path", Napi::String::New(env_, f.file->path));
      if (f.file->index) file.Set("index", Napi::Number::New(env_, *f.file->index));
      out.Set("file", file);
    }

    if (request.wants(Field::FacesVariable)) out.Set("variable", Napi::Boolean::New(env_, f.variable));
    if (request.wants(Field::FacesAxes) && f.axes) out.Set("axes", axes(f.axes));
    if (request.wants(Field::FacesCoordinates) && !f.coordinates.empty()) {
      auto coordinates = Napi::Object::New(env_);
      for (const auto& [tag, coordinate] : f.coordinates) coordinates.Set(tag, Napi::Number::New(env_, coordinate));
      out.Set("coordinates", coordinates);
    }
    if (request.wants(Field::FacesNamedInstanceIndex) && f.namedInstanceIndex) {
      out.Set("namedInstanceIndex", Napi::Number::New(env_, *f.namedInstanceIndex));
    }

    // The backend builds only the entries of the request (Request::openTypeSelection), so the object is whole as it is.
    if (request.wantsAnyOpenType() && f.opentype) out.Set("opentype", shared(f.opentype));
    if (request.wants(Field::FacesCoverage) && f.coverage) {
      auto ranges = Napi::Array::New(env_, f.coverage->size());
      for (size_t i = 0; i < f.coverage->size(); ++i) {
        auto range = Napi::Array::New(env_, 2);
        range.Set(0u, Napi::Number::New(env_, (*f.coverage)[i].first));
        range.Set(1u, Napi::Number::New(env_, (*f.coverage)[i].second));
        ranges.Set(static_cast<uint32_t>(i), range);
      }
      out.Set("coverage", ranges);
    }

    if (request.platformObject) out.Set("platform", platform(f.platform));
    return out;
  }

  Napi::Object family(const Family& f) {
    const Request& request = *request_;
    auto out = Napi::Object::New(env_);
    out.Set("family", Napi::String::New(env_, f.family));
    if (request.wants(Field::FamilyNames)) out.Set("familyNames", localized(f.familyNames));
    if (request.wantsAnyFace()) {
      auto faces = Napi::Array::New(env_, f.faces.size());
      for (size_t i = 0; i < f.faces.size(); ++i) faces.Set(static_cast<uint32_t>(i), face(f.faces[i]));
      out.Set("faces", faces);
    }
    if (!request.orthographies.empty()) {
      auto counts = Napi::Array::New(env_, f.languageCoverage.size());
      for (size_t i = 0; i < f.languageCoverage.size(); ++i) {
        counts.Set(static_cast<uint32_t>(i), Napi::Number::New(env_, f.languageCoverage[i]));
      }
      out.Set("languageCoverage", counts);
    }
    return out;
  }

  static constexpr std::string_view kPlatformPrefix = "faces.platform.";

  Napi::Env env_;
  const Request* request_;
  std::set<std::string, std::less<>> platformKeys_;  // the requested `faces.platform.*` fields, without the prefix
  std::unordered_map<const Value*, Napi::Reference<Napi::Object>> sharedObjects_;
  std::unordered_map<const std::vector<VariationAxis>*, Napi::Reference<Napi::Object>> sharedAxes_;
};

}  // namespace

Napi::Array familiesToJs(Napi::Env env, const std::vector<Family>& families, const Request& request) {
  return Converter(env, &request).families(families);
}

Napi::Value sharedToJs(Napi::Env env, const Value::Shared& shared) { return Converter(env).shared(shared); }

}  // namespace sfi
