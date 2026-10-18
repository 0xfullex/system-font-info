#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>

#include "../../core/request.h"
#include "dwrite.h"

namespace sfi::platform::win32 {

struct Source {
  std::optional<uint32_t> type;
  std::optional<std::string> name;
};

// One call's WSS collection. Resource joins deliberately ignore simulations: synthesized members use their base source.
class Collection {
 public:
  explicit Collection(const Request& request);
  IDWriteFactory6* factory() const { return factory_.Get(); }
  IDWriteFontCollection* fonts() const { return collection_.Get(); }
  bool variable(const ResourceKey& key);
  Source source(const ResourceKey& key);
  bool processFont(const std::string& path) const;

 private:
  void loadResources();
  void loadSources();

  const Request& request_;
  ComPtr<IDWriteFactory6> factory_;
  ComPtr<IDWriteFontCollection> collection_;
  ComPtr<IDWriteFontSet1> systemSet_;
  ComPtr<IDWriteFontSet1> memberSet_;
  std::set<std::wstring> processPaths_;
  bool resourcesLoaded_ = false;
  bool sourcesLoaded_ = false;
  std::map<ResourceKey, bool> variable_;
  std::map<ResourceKey, Source> sources_;
};

}  // namespace sfi::platform::win32
