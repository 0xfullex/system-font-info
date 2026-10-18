#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../core/model.h"
#include "../../core/request.h"
#include "../../sfnt/sfnt.h"
#include "cf.h"

namespace sfi::platform::darwin {

// Another font file the collection lists under the PostScript name of a face.
struct ShadowedFile {
  std::string path;
  bool variable = false;
};

// Builds one `Face` from the font descriptor CoreText resolves a PostScript
// name to, doing only the work the request needs (docs/platforms.md, "Source
// groups"): the descriptor attributes (B) that a requested field is made of;
// the font, the table directory, the `name`, `fvar` and `cmap` tables (C), only
// for the first field of a face that needs them; the `opentype` entries
// requested (D); the character set of `faces.coverage` (E). The reader keeps
// what several faces of one font resource share (the parsed tables, the
// `opentype` value, the variation axes, the face index lookups of a collection
// file), so one instance serves a whole enumeration.
class FaceReader {
 public:
  FaceReader(const Request& request, WorkLog& work);

  // `enabled` is whether the face's PostScript name is listed when CoreText leaves the disabled fonts out; the
  // descriptor of a disabled font cannot say (see `enumerate.cpp`).
  Face read(CTFontDescriptorRef descriptor, const std::string& postscriptName, bool enabled,
            const std::vector<ShadowedFile>& shadowed);

 private:
  // What is shared by every face of one font resource (one face of a file, or
  // all named instances of one variable font).
  struct Resource {
    sfnt::FontTables tables;
    Value::Shared opentype;  // the requested entries; null when the request wants none
    SharedAxes axes;         // null when the font has no variation axes or the request needs none
  };

  // The `name` tables of every face of a collection file, for finding a face's
  // index by comparing tables.
  struct FileFaces {
    std::optional<sfnt::FontFile> file;
    std::vector<std::vector<uint8_t>> nameTables;
  };

  // Where the tables of one face come from, and which face of its file it is.
  struct TableSource {
    std::vector<uint32_t> tags;
    sfnt::TableFetcher fetch;
    std::optional<uint32_t> index;
  };

  // A face's `CTFont` and what was read through it (group C).
  struct OpenedFont {
    Ref<CTFontRef> font;
    // CoreText created another font than the descriptor names. Whatever a CTFont
    // provides (`features`, `metrics`, `CTFontCopyVariation`) is then not the
    // face's.
    bool substituted = false;
    std::string path;               // "" when the descriptor has no file URL
    std::optional<uint32_t> index;  // set when the request wants `faces.file`, or the tables came from the file
    bool isSfnt = false;            // the font has tables
    std::shared_ptr<const Resource> resource;
  };

  OpenedFont openFont(CTFontDescriptorRef descriptor, const std::string& postscriptName);
  std::shared_ptr<const Resource> loadResource(const TableSource& source, CTFontDescriptorRef descriptor,
                                               const std::string& path, std::optional<uint32_t> index);
  TableSource tablesFromFont(CTFontRef font, const std::string& path);
  TableSource tablesFromFile(const std::string& path, const std::string& postscriptName);
  std::optional<uint32_t> faceIndexInFile(const std::string& path, const std::vector<uint32_t>& tags, CTFontRef font);
  FileFaces& fileFaces(const std::string& path);
  std::optional<Value> featuresOf(const OpenedFont& opened);

  const Request& request_;
  WorkLog& work_;
  // A requested field is made of the descriptor's traits.
  bool wantsTraits_ = false;
  // A requested field depends on which instance of a variable font the face is: the coordinates, the named instance
  // index, and the names (a named instance has its own style and full names).
  bool wantsInstance_ = false;
  std::map<std::string, FileFaces> files_;
  std::map<std::pair<std::string, uint32_t>, std::shared_ptr<const Resource>> resources_;
  // CTFontCopyFeatures per font resource: it is the most expensive call per
  // face, and the named instances of one variable font report the same list.
  std::map<std::pair<std::string, uint32_t>, std::optional<Value>> features_;
};

}  // namespace sfi::platform::darwin
