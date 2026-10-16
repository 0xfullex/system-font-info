#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../core/model.h"
#include "../../sfnt/sfnt.h"
#include "appkit.h"
#include "cf.h"

namespace sfi::platform::darwin {

// Another font file the collection lists under the PostScript name of a face.
struct ShadowedFile {
  std::string path;
  bool variable = false;
};

// Builds one `Face` from the font descriptor CoreText resolves a PostScript
// name to. The reader keeps what several faces of one font resource share
// (the parsed tables, the variation axes, the face index lookups of a
// collection file), so one instance serves a whole enumeration.
class FaceReader {
 public:
  explicit FaceReader(const EnumerateOptions& options) : options_(options) {}

  // `appKitMember` is the face's entry in NSFontManager's member list of its
  // family, when it has one.
  Face read(CTFontDescriptorRef descriptor, const std::vector<ShadowedFile>& shadowed,
            const AppKitMember* appKitMember);

 private:
  // What is shared by every face of one font resource (one face of a file, or
  // all named instances of one variable font).
  struct Resource {
    sfnt::FontTables tables;
    SharedAxes axes;  // null when the font has no variation axes
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

  std::shared_ptr<const Resource> loadResource(const TableSource& source, CTFontDescriptorRef descriptor,
                                               const std::optional<FileLocation>& file);
  TableSource tablesFromFont(CTFontRef font, const std::string& path);
  TableSource tablesFromFile(const std::string& path, const std::string& postscriptName);
  std::optional<uint32_t> faceIndexInFile(const std::string& path, const std::vector<uint32_t>& tags, CTFontRef font);
  FileFaces& fileFaces(const std::string& path);
  std::optional<Value> featuresOf(CTFontRef font, const std::optional<FileLocation>& file);

  EnumerateOptions options_;
  std::map<std::string, FileFaces> files_;
  std::map<std::pair<std::string, uint32_t>, std::shared_ptr<const Resource>> resources_;
  // CTFontCopyFeatures per font resource: it is the most expensive call per
  // face, and the named instances of one variable font report the same list.
  std::map<std::pair<std::string, uint32_t>, std::optional<Value>> features_;
};

}  // namespace sfi::platform::darwin
