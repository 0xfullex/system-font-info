#pragma once

#include <fontconfig/fontconfig.h>

#include <map>
#include <set>

#include "../../core/platform.h"
#include "fc.h"

namespace sfi::platform::linux_backend {

class FaceReader {
 public:
  FaceReader(const Request& request, WorkLog& work, const std::set<Identity>& variableResources)
      : request_(request), work_(work), variableResources_(variableResources) {}
  // `inApplicationSet`: whether the pattern's font is in the configuration's application set (`FcSetApplication`).
  Face read(FcPattern* pattern, bool inApplicationSet);

 private:
  struct Resource {
    sfnt::FontTables tables;
    Value::Shared opentype;
    SharedAxes axes;
  };
  const Resource& readResource(const Identity& id);

  const Request& request_;
  WorkLog& work_;
  const std::set<Identity>& variableResources_;
  std::map<Identity, Resource> resources_;
};

}  // namespace sfi::platform::linux_backend
