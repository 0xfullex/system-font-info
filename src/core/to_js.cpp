#include "to_js.h"

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
  explicit Converter(Napi::Env env) : env_(env) {}

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
    for (size_t i = 0; i < strings.size(); ++i) array.Set(static_cast<uint32_t>(i), Napi::String::New(env_, strings[i]));
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

  Napi::Object face(const Face& f) {
    auto out = Napi::Object::New(env_);
    out.Set("postscriptName", Napi::String::New(env_, f.postscriptName));
    out.Set("fullNames", localized(f.fullNames));
    out.Set("styleNames", localized(f.styleNames));
    out.Set("familyNames", localized(f.familyNames));
    out.Set("weight", Napi::Number::New(env_, f.weight));
    out.Set("width", Napi::Number::New(env_, f.width));
    out.Set("style", Napi::String::New(env_, styleName(f.style)));
    out.Set("monospace", Napi::Boolean::New(env_, f.monospace));
    out.Set("color", Napi::Boolean::New(env_, f.color));
    out.Set("symbol", Napi::Boolean::New(env_, f.symbol));
    out.Set("enabled", Napi::Boolean::New(env_, f.enabled));
    out.Set("hidden", Napi::Boolean::New(env_, f.hidden));

    auto simulations = Napi::Object::New(env_);
    simulations.Set("bold", Napi::Boolean::New(env_, f.simulatedBold));
    simulations.Set("oblique", Napi::Boolean::New(env_, f.simulatedOblique));
    out.Set("simulations", simulations);

    out.Set("languages", stringArray(f.languages));
    out.Set("scope", Napi::String::New(env_, scopeName(f.scope)));
    out.Set("format", Napi::String::New(env_, formatName(f.format)));
    if (f.file) {
      auto file = Napi::Object::New(env_);
      file.Set("path", Napi::String::New(env_, f.file->path));
      file.Set("index", Napi::Number::New(env_, f.file->index));
      out.Set("file", file);
    }

    out.Set("variable", Napi::Boolean::New(env_, f.variable));
    if (f.axes) out.Set("axes", axes(f.axes));
    if (!f.coordinates.empty()) {
      auto coordinates = Napi::Object::New(env_);
      for (const auto& [tag, coordinate] : f.coordinates) coordinates.Set(tag, Napi::Number::New(env_, coordinate));
      out.Set("coordinates", coordinates);
    }
    if (f.namedInstance) out.Set("namedInstance", Napi::Number::New(env_, *f.namedInstance));

    if (f.opentype) out.Set("opentype", shared(f.opentype));
    if (f.coverage) {
      auto ranges = Napi::Array::New(env_, f.coverage->size());
      for (size_t i = 0; i < f.coverage->size(); ++i) {
        auto range = Napi::Array::New(env_, 2);
        range.Set(0u, Napi::Number::New(env_, (*f.coverage)[i].first));
        range.Set(1u, Napi::Number::New(env_, (*f.coverage)[i].second));
        ranges.Set(static_cast<uint32_t>(i), range);
      }
      out.Set("coverage", ranges);
    }

    out.Set("platform", value(f.platform));
    return out;
  }

  Napi::Object family(const Family& f) {
    auto out = Napi::Object::New(env_);
    out.Set("family", Napi::String::New(env_, f.family));
    auto faces = Napi::Array::New(env_, f.faces.size());
    for (size_t i = 0; i < f.faces.size(); ++i) faces.Set(static_cast<uint32_t>(i), face(f.faces[i]));
    out.Set("faces", faces);
    return out;
  }

  Napi::Env env_;
  std::unordered_map<const Value*, Napi::Reference<Napi::Object>> sharedObjects_;
  std::unordered_map<const std::vector<VariationAxis>*, Napi::Reference<Napi::Object>> sharedAxes_;
};

}  // namespace

Napi::Array familiesToJs(Napi::Env env, const std::vector<Family>& families) {
  return Converter(env).families(families);
}

Napi::Value sharedToJs(Napi::Env env, const Value::Shared& shared) { return Converter(env).shared(shared); }

}  // namespace sfi
