#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "value.h"

// The native result of an enumeration. Every platform backend fills the same
// structs, so all platforms produce one shape. Field meanings are documented
// on the public TypeScript types (ts/types.ts); notes here cover only what is
// specific to the native side.
//
// The native layer reports facts; the JS layer derives the rest: it
// canonicalizes language tags, merges duplicates, picks the default strings
// (fullName, styleName, axis name), computes family-level aggregates and sorts
// families.

namespace sfi {

// `language` is a BCP 47 tag as the native side produced it (mapped from an
// LCID, a Mac language code, a platform locale name...). It is not yet
// canonicalized. "" when the source gives no language.
struct LocalizedString {
  std::string language;
  std::string value;
};
using LocalizedStrings = std::vector<LocalizedString>;

struct VariationAxis {
  std::string tag;
  LocalizedStrings names;  // empty when the font names no axis name
  double min = 0;
  double defaultValue = 0;
  double max = 0;
  bool hidden = false;
};
using SharedAxes = std::shared_ptr<const std::vector<VariationAxis>>;

struct FileLocation {
  std::string path;    // UTF-8
  // Face index inside a collection; 0 for a single-font file. Unset when the
  // platform cannot tell which face of the file this is.
  std::optional<uint32_t> index;
};

enum class Style { Normal, Italic, Oblique };
enum class Scope { System, User, Process, Unknown };
enum class Format { TrueType, Cff, Cff2, Type1, Bitmap, Unknown };

struct Face {
  std::string postscriptName;
  LocalizedStrings fullNames;
  LocalizedStrings styleNames;
  LocalizedStrings familyNames;

  double weight = 400;  // CSS 1-1000
  double width = 100;   // CSS percentage 50-200
  Style style = Style::Normal;
  bool monospace = false;
  bool color = false;
  bool symbol = false;
  bool enabled = true;
  bool hidden = false;
  bool simulatedBold = false;
  bool simulatedOblique = false;
  std::vector<std::string> languages;
  Scope scope = Scope::Unknown;
  Format format = Format::Unknown;
  std::optional<FileLocation> file;

  bool variable = false;
  SharedAxes axes;  // non-null iff variable
  // Every axis of `axes`, in axis order; non-empty iff variable.
  std::vector<std::pair<std::string, double>> coordinates;
  std::optional<uint32_t> namedInstance;

  Value::Shared opentype;  // null when tables were not requested or the face is not an sfnt
  // Inclusive code point ranges, ascending; set only when coverage was requested.
  std::optional<std::vector<std::pair<uint32_t, uint32_t>>> coverage;

  Value platform = Value::object();  // first key is "os"
};

struct Family {
  std::string family;       // the name the platform matcher groups by
  std::vector<Face> faces;  // in the platform's member order
};

struct EnumerateOptions {
  bool tables = true;
  bool coverage = false;
};

}  // namespace sfi
