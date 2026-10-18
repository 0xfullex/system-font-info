#pragma once

#include <fontconfig/fontconfig.h>

#include <map>
#include <set>

#include "../../core/platform.h"
#include "fc.h"

namespace sfi::platform::linux_backend {

class FaceReader {
 public:
  // `members` are the patterns to be read, in listing order; see `lists`.
  FaceReader(const Request& request, WorkLog& work, const std::set<Identity>& variableResources,
             const std::vector<FcPattern*>& members);
  // Whether the face of `pattern` is listed. All faces are, except those of a variable resource whose `fvar` cannot
  // be read: its named instances would have no coordinates, so the identity could not tell them apart, and only one
  // face of the resource is kept. That is its default instance (no named-instance bits in `FC_INDEX`), else the first
  // member. This is the one place the rule lives; `read` is called only for the faces it lists. A request that does
  // not ask for the coordinates (the identity's part that needs the file) lists every face and reads nothing here.
  bool lists(FcPattern* pattern);
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
  std::map<Identity, FcPattern*> keptWithoutFvar_;  // by resource
};

}  // namespace sfi::platform::linux_backend
