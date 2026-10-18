#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "../../core/request.h"
#include "../../sfnt/sfnt.h"
#include "collection.h"

namespace sfi::platform::win32 {

struct ReadFace {
  Face face;
  std::string identity;
};

// All resources live for this enumeration only. The first field needing an object or tables opens it; B-only faces
// never create a face, resource, or file reader. Shared data is keyed by the physical file and collection index.
class FaceReader {
 public:
  FaceReader(const Request& request, WorkLog& work, Collection& collection);
  ReadFace read(IDWriteFont* font, const LocalizedStrings& familyNames, bool inactive);

 private:
  struct Resource {
    ComPtr<IDWriteFontFace3> face;
    ComPtr<IDWriteFontResource> resource;
    std::optional<std::vector<uint32_t>> tags;  // The table directory, from the file; empty when unreadable.
    std::optional<sfnt::FontTables> tables;
    Value::Shared opentype;
    SharedAxes axes;
    std::optional<Value> resourceAxes;
  };

  Resource& resource(const Reference& reference);
  IDWriteFontFace3* openFace(Resource& resource, const Reference& reference);
  const std::vector<uint32_t>& tableTags(Resource& resource, const Reference& reference);
  const sfnt::FontTables& tables(Resource& resource, const Reference& reference);
  const Value& resourceAxes(Resource& resource, const Reference& reference);
  SharedAxes axes(Resource& resource, const Reference& reference);

  const Request& request_;
  WorkLog& work_;
  Collection& collection_;
  std::map<ResourceKey, Resource> resources_;
  std::map<std::string, std::optional<sfnt::FontFile>> files_;
};

}  // namespace sfi::platform::win32
