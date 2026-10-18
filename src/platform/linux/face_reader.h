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
  Face read(FcPattern* pattern);

 private:
  struct Resource {
    sfnt::FontTables tables;
    Value::Shared opentype;
    SharedAxes axes;
  };
  const Resource& readResource(const Identity& id, const std::string& wrapper);

  const Request& request_;
  WorkLog& work_;
  const std::set<Identity>& variableResources_;
  std::map<Identity, Resource> resources_;
};

}  // namespace sfi::platform::linux_backend
