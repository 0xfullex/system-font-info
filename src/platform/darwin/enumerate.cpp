#include <algorithm>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "../../core/platform.h"
#include "autorelease_pool.h"
#include "cf.h"
#include "face_reader.h"
#include "language_coverage.h"
#include "variations.h"

// enumerateFonts: the installed fonts as CoreText's font collection lists them.
//
// The collection lists a font once for each file that provides it, so a
// PostScript name can appear more than once (a system update ships a new
// version of a font next to the old one). CSS local(), font family matching and
// every API that takes a PostScript name resolve it to exactly one of them, so
// the result holds one face per PostScript name: the one the name resolves to.
// The others are not dropped silently: they are reported under
// `platform.shadowed` of that face.
//
// Two ways to get the descriptors to group. Without `request.families` it is the
// whole collection. With it, it is what CoreText's family lookup returns for
// each requested name, which is far less work for a few families; the families
// it yields are then selected with `request.wantsFamily`.

namespace sfi::platform {
namespace {

using namespace darwin;

// All collection descriptors that share one PostScript name.
struct Group {
  std::string postscriptName;
  std::vector<CTFontDescriptorRef> descriptors;  // owned by the collection's array
};

// The options of the listings that include the disabled fonts, which are installed fonts too. Null means CoreText's
// defaults, which leave them out.
Ref<CFDictionaryRef> includingDisabledFonts() {
  const void* keys[] = {kCTFontCollectionIncludeDisabledFontsOption};
  const void* values[] = {kCFBooleanTrue};
  return Ref<CFDictionaryRef>(CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks));
}

Ref<CFArrayRef> copyCollectionDescriptors(bool includeDisabled) {
  const Ref<CFDictionaryRef> options = includeDisabled ? includingDisabledFonts() : Ref<CFDictionaryRef>();
  const Ref<CTFontCollectionRef> collection(CTFontCollectionCreateFromAvailableFonts(options.get()));
  if (!collection) throw std::runtime_error("system-font-info: CoreText cannot create the font collection");
  Ref<CFArrayRef> descriptors(CTFontCollectionCreateMatchingFontDescriptors(collection.get()));
  if (!descriptors) throw std::runtime_error("system-font-info: CoreText cannot list the font collection");
  return descriptors;
}

std::string fileOf(CTFontDescriptorRef descriptor) {
  return filePath(copyAttribute(descriptor, kCTFontURLAttribute).get());
}

std::string postscriptNameOf(CTFontDescriptorRef descriptor) {
  return toUtf8(asString(copyAttribute(descriptor, kCTFontNameAttribute).get()));
}

// The faces CoreText lists when it leaves the disabled fonts out, each as PostScript name and file. A disabled font
// still reports `kCTFontEnabledAttribute` 1, with a priority and a registration scope CoreText makes up for it, so
// being absent from this listing is the only sign that a face is disabled.
class EnabledFaces {
 public:
  // `descriptors` may be null: CoreText returns nothing when no font matches.
  explicit EnabledFaces(CFArrayRef descriptors) {
    for (CFIndex i = 0; descriptors && i < CFArrayGetCount(descriptors); ++i) {
      keys_.insert(keyOf(static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(descriptors, i))));
    }
  }

  bool contains(CTFontDescriptorRef descriptor) const { return keys_.count(keyOf(descriptor)) != 0; }

 private:
  static std::string keyOf(CTFontDescriptorRef descriptor) {
    return postscriptNameOf(descriptor) + '\0' + fileOf(descriptor);
  }

  std::unordered_set<std::string> keys_;
};

// Only `enabled` and `scope` depend on a face being enabled, and finding out costs a second listing.
bool needsEnabledState(const Request& request) {
  return request.wants(Field::FacesEnabled) || request.wants(Field::FacesScope);
}

// Groups the descriptors by PostScript name, in the order the names first
// appear. A descriptor without a name cannot be matched with another one, so it
// forms a group of its own.
std::vector<Group> groupByPostscriptName(CFArrayRef descriptors) {
  std::vector<Group> groups;
  std::unordered_map<std::string, size_t> groupOfName;
  for (CFIndex i = 0; i < CFArrayGetCount(descriptors); ++i) {
    auto descriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(descriptors, i));
    std::string name = postscriptNameOf(descriptor);
    if (!name.empty()) {
      const auto [entry, inserted] = groupOfName.try_emplace(name, groups.size());
      if (!inserted) {
        groups[entry->second].descriptors.push_back(descriptor);
        continue;
      }
    }
    groups.push_back({std::move(name), {descriptor}});
  }
  return groups;
}

// What the PostScript name resolves to, as a descriptor of that very name. Null when resolution comes up empty (a
// disabled font is not matched) or lands on a descriptor of another name.
CTFontDescriptorRef resolveByName(const std::string& postscriptName, Ref<CTFontDescriptorRef>& resolvedStorage) {
  if (postscriptName.empty()) return nullptr;
  const auto name = makeString(postscriptName);
  const Ref<CTFontDescriptorRef> byName(CTFontDescriptorCreateWithNameAndSize(name.get(), 0));
  resolvedStorage = Ref<CTFontDescriptorRef>(CTFontDescriptorCreateMatchingFontDescriptor(byName.get(), nullptr));
  return resolvedStorage && postscriptNameOf(resolvedStorage.get()) == postscriptName ? resolvedStorage.get() : nullptr;
}

// The collection's descriptors grouped by PostScript name, listed on first use: the full listing needs all of it, and
// the family lookup path only the names it has to settle against the collection.
class CollectionGroups {
 public:
  const std::vector<Group>& all() {
    load();
    return groups_;
  }

  // The group of a PostScript name; null when the collection has none.
  const Group* find(const std::string& postscriptName) {
    load();
    const auto found = groupOfName_.find(postscriptName);
    return found == groupOfName_.end() ? nullptr : &groups_[found->second];
  }

 private:
  void load() {
    if (descriptors_) return;
    descriptors_ = copyCollectionDescriptors(true);
    groups_ = groupByPostscriptName(descriptors_.get());
    for (size_t i = 0; i < groups_.size(); ++i) {
      if (!groups_[i].postscriptName.empty()) groupOfName_.emplace(groups_[i].postscriptName, i);
    }
  }

  Ref<CFArrayRef> descriptors_;  // keeps the descriptors of the groups alive
  std::vector<Group> groups_;
  std::unordered_map<std::string, size_t> groupOfName_;
};

// A face that is ready to be read, with what the family-level steps decided.
struct Seed {
  Ref<CTFontDescriptorRef> resolved;  // keeps the descriptor alive when it did not come from the collection
  CTFontDescriptorRef descriptor = nullptr;
  std::string postscriptName;
  bool enabled = true;
  std::vector<ShadowedFile> shadowed;
};

struct FamilySeeds {
  std::string name;
  std::vector<Seed> seeds;  // in collection order, or in CoreText's member order when `ordered`
  bool ordered = false;
};

std::vector<ShadowedFile> shadowedFiles(const Group& group, CTFontDescriptorRef chosen) {
  const std::string chosenFile = fileOf(chosen);
  std::vector<ShadowedFile> shadowed;
  for (CTFontDescriptorRef other : group.descriptors) {
    if (other == chosen) continue;
    std::string file = fileOf(other);
    // The same file listed twice is one font, not a shadowed one.
    if (file == chosenFile || file.empty()) continue;
    if (std::any_of(shadowed.begin(), shadowed.end(), [&](const ShadowedFile& s) { return s.path == file; })) continue;
    shadowed.push_back({std::move(file), hasVariationAxes(other)});
  }
  return shadowed;
}

// Groups the resolved faces by family, in the order the families first appear. The shadowed files, which cost a
// look at every other descriptor of a group, are found only when the request wants them. A face is enabled unless
// `enabledFaces`, the listing without disabled fonts of the same scope as `groups`, is given and lacks it.
//
// `groups` come from the whole collection, or from a family lookup, which lists one file for a PostScript name. For a
// lookup, `collection` is the whole collection: its group of the name stands in where the lookup's is not enough, that
// is when the request wants the shadowed files, and when resolution finds nothing and the face falls back to the first
// descriptor of the name, which must be the one the full listing falls back to.
std::vector<FamilySeeds> groupByFamily(const std::vector<Group>& groups, const Request& request,
                                       CollectionGroups* collection, const EnabledFaces* enabledFaces) {
  std::vector<FamilySeeds> families;
  std::unordered_map<std::string, size_t> familyOfName;
  for (const Group& group : groups) {
    Seed seed;
    seed.postscriptName = group.postscriptName;
    CTFontDescriptorRef resolved = resolveByName(group.postscriptName, seed.resolved);
    const Group* members = &group;
    if (collection && (!resolved || request.wants(Field::FacesPlatformShadowed))) {
      if (const Group* inCollection = collection->find(group.postscriptName)) members = inCollection;
    }
    seed.descriptor = resolved ? resolved : members->descriptors.front();
    seed.enabled = !enabledFaces || enabledFaces->contains(seed.descriptor);
    if (request.wants(Field::FacesPlatformShadowed)) seed.shadowed = shadowedFiles(*members, seed.descriptor);

    std::string family = toUtf8(asString(copyAttribute(seed.descriptor, kCTFontFamilyNameAttribute).get()));
    const auto [entry, inserted] = familyOfName.try_emplace(family, families.size());
    if (inserted) families.push_back({std::move(family), {}});
    families[entry->second].seeds.push_back(std::move(seed));
  }
  return families;
}

// The descriptors CoreText's family lookup returns for `family`, in the order its matcher lists them, which is the
// family's member order. Disabled fonts are included when `includeDisabled`. Null when CoreText cannot make the lookup
// or no font matches.
Ref<CFArrayRef> lookUpFamily(const std::string& family, bool includeDisabled) {
  const auto familyName = makeString(family);
  const void* attributeKeys[] = {kCTFontFamilyNameAttribute};
  const void* attributeValues[] = {familyName.get()};
  const Ref<CFDictionaryRef> attributes(CFDictionaryCreate(kCFAllocatorDefault, attributeKeys, attributeValues, 1,
                                                           &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks));
  const Ref<CTFontDescriptorRef> descriptor(CTFontDescriptorCreateWithAttributes(attributes.get()));
  const void* descriptorList[] = {descriptor.get()};
  const Ref<CFArrayRef> descriptors(CFArrayCreate(kCFAllocatorDefault, descriptorList, 1, &kCFTypeArrayCallBacks));
  const Ref<CFDictionaryRef> options = includeDisabled ? includingDisabledFonts() : Ref<CFDictionaryRef>();
  const Ref<CTFontCollectionRef> collection(
      CTFontCollectionCreateWithFontDescriptors(descriptors.get(), options.get()));
  if (!collection) return {};
  return Ref<CFArrayRef>(CTFontCollectionCreateMatchingFontDescriptors(collection.get()));
}

// The family's members in CoreText's member order first, then the faces that
// lookup does not name, in collection order.
void orderLikeCoreText(FamilySeeds& family) {
  if (family.ordered) return;
  family.ordered = true;

  // A PostScript name counts once, at its first position.
  std::vector<std::string> names;
  if (const Ref<CFArrayRef> members = lookUpFamily(family.name, true)) {
    for (CFIndex i = 0; i < CFArrayGetCount(members.get()); ++i) {
      std::string name = postscriptNameOf(static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(members.get(), i)));
      if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end()) names.push_back(std::move(name));
    }
  }

  std::vector<Seed> ordered;
  std::vector<bool> placed(family.seeds.size(), false);
  for (const std::string& name : names) {
    for (size_t i = 0; i < family.seeds.size(); ++i) {
      if (placed[i] || family.seeds[i].postscriptName != name) continue;
      placed[i] = true;
      ordered.push_back(std::move(family.seeds[i]));
      break;
    }
  }
  for (size_t i = 0; i < family.seeds.size(); ++i) {
    if (!placed[i]) ordered.push_back(std::move(family.seeds[i]));
  }
  family.seeds = std::move(ordered);
}

// The faces of every family of the collection.
std::vector<FamilySeeds> familiesOfCollection(const Request& request, CollectionGroups& collection) {
  std::optional<EnabledFaces> enabledFaces;
  if (needsEnabledState(request)) enabledFaces.emplace(copyCollectionDescriptors(false).get());
  return groupByFamily(collection.all(), request, nullptr, enabledFaces ? &*enabledFaces : nullptr);
}

// The faces of each requested family, from the family lookup. CoreText's lookup is case-insensitive, matches localized
// and legacy names and can return another family (`Kefa` returns `Kefa III`), so the families it yields are filtered
// with `request.wantsFamily`, the one exact match, and a family that several lookups yield (`Kefa` and `Kefa III`
// both requested) is kept once, from the lookup of its own name: that is the one whose order is its member order.
// The groups of a lookup come out in its order, so the faces of such a family are in member order already; the faces
// the full collection would add after them (those the lookup does not name) are not looked for. The lookup lists one
// file for a PostScript name; the collection settles the names where that is not enough (see `groupByFamily`).
// `lookups` keeps the lookups' descriptors alive.
std::vector<FamilySeeds> familiesOfLookups(const Request& request, CollectionGroups& collection,
                                           std::vector<Ref<CFArrayRef>>& lookups) {
  std::vector<FamilySeeds> result;
  std::unordered_map<std::string, size_t> resultOfFamily;
  std::vector<std::string> looked;
  for (const std::string& name : *request.families) {
    if (std::find(looked.begin(), looked.end(), name) != looked.end()) continue;
    looked.push_back(name);

    Ref<CFArrayRef> members = lookUpFamily(name, true);
    if (!members) continue;
    // The same lookup without the disabled fonts: the faces it lacks are the disabled ones.
    std::optional<EnabledFaces> enabledFaces;
    if (needsEnabledState(request)) enabledFaces.emplace(lookUpFamily(name, false).get());
    for (FamilySeeds& family : groupByFamily(groupByPostscriptName(members.get()), request, &collection,
                                             enabledFaces ? &*enabledFaces : nullptr)) {
      if (!request.wantsFamily(family.name)) continue;
      // A family of another name is in this lookup's order, not in its member order; it is put in member order
      // later, unless the lookup of its own name comes by.
      family.ordered = family.name == name;
      const auto [entry, inserted] = resultOfFamily.try_emplace(family.name, result.size());
      if (inserted) {
        result.push_back(std::move(family));
      } else if (family.ordered && !result[entry->second].ordered) {
        result[entry->second] = std::move(family);
      }
    }
    lookups.push_back(std::move(members));
  }
  return result;
}

// How many code points of each requested orthography the family's default face covers. When the matcher returns no
// face of the family, the first face of its member order stands in.
std::vector<uint32_t> languageCoverageOf(FamilySeeds& family, const Request& request, WorkLog& work) {
  const Ref<CTFontDescriptorRef> matched = matchDefaultFace(family.name);
  CTFontDescriptorRef face = matched.get();
  if (!face) {
    orderLikeCoreText(family);
    face = family.seeds.front().descriptor;
  }
  ++work.characterSets;
  return countLanguageCoverage(face, request.orthographies);
}

std::vector<Family> enumerate(const Request& request, WorkLog& work) {
  // Keep the descriptors the seeds point into alive.
  CollectionGroups collection;
  std::vector<Ref<CFArrayRef>> lookups;
  std::vector<FamilySeeds> families =
      request.families ? familiesOfLookups(request, collection, lookups) : familiesOfCollection(request, collection);

  FaceReader reader(request, work);
  std::vector<Family> result;
  for (FamilySeeds& seeds : families) {
    Family family;
    family.family = seeds.name;
    if (request.wantsAnyFace()) {
      orderLikeCoreText(seeds);
      for (const Seed& seed : seeds.seeds) {
        family.faces.push_back(reader.read(seed.descriptor, seed.postscriptName, seed.enabled, seed.shadowed));
      }
    }
    if (!request.orthographies.empty()) family.languageCoverage = languageCoverageOf(seeds, request, work);
    result.push_back(std::move(family));
  }
  return result;
}

}  // namespace

std::vector<Family> enumerateFonts(const Request& request, WorkLog& work) {
  if (request.families && request.families->empty()) return {};
  std::vector<Family> result;
  runInAutoreleasePool([&] { result = enumerate(request, work); });
  return result;
}

}  // namespace sfi::platform
