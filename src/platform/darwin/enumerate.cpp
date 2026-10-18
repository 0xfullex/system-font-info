#include <algorithm>
#include <map>
#include <stdexcept>
#include <unordered_map>

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
// each requested name, which is far less work for a few families: the lookup
// matches case-insensitively and by localized and legacy names, and can return
// another family (`Kefa` returns `Kefa III`), so the families it yields are
// then kept only when their `family` is exactly the requested name.

namespace sfi::platform {
namespace {

using namespace darwin;

// All collection descriptors that share one PostScript name.
struct Group {
  std::string postscriptName;
  std::vector<CTFontDescriptorRef> descriptors;  // owned by the collection's array
};

// Disabled fonts are installed fonts too.
Ref<CFDictionaryRef> collectionOptions() {
  const void* keys[] = {kCTFontCollectionIncludeDisabledFontsOption};
  const void* values[] = {kCFBooleanTrue};
  return Ref<CFDictionaryRef>(CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks));
}

Ref<CFArrayRef> copyCollectionDescriptors() {
  const Ref<CFDictionaryRef> options = collectionOptions();
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

// What the PostScript name resolves to, as a descriptor of that very name.
// Resolution can come up empty (a disabled font is not matched) or land on a
// descriptor of another name; the first descriptor of the group then stands.
CTFontDescriptorRef resolve(const Group& group, Ref<CTFontDescriptorRef>& resolvedStorage) {
  if (!group.postscriptName.empty()) {
    const auto name = makeString(group.postscriptName);
    const Ref<CTFontDescriptorRef> byName(CTFontDescriptorCreateWithNameAndSize(name.get(), 0));
    resolvedStorage = Ref<CTFontDescriptorRef>(CTFontDescriptorCreateMatchingFontDescriptor(byName.get(), nullptr));
    if (resolvedStorage && postscriptNameOf(resolvedStorage.get()) == group.postscriptName)
      return resolvedStorage.get();
  }
  return group.descriptors.front();
}

// A face that is ready to be read, with what the family-level steps decided.
struct Seed {
  Ref<CTFontDescriptorRef> resolved;  // keeps the descriptor alive when it did not come from the collection
  CTFontDescriptorRef descriptor = nullptr;
  std::string postscriptName;
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
// look at every other descriptor of a group, are found only when the request wants them.
std::vector<FamilySeeds> groupByFamily(const std::vector<Group>& groups, const Request& request) {
  std::vector<FamilySeeds> families;
  std::unordered_map<std::string, size_t> familyOfName;
  for (const Group& group : groups) {
    Seed seed;
    seed.descriptor = resolve(group, seed.resolved);
    seed.postscriptName = group.postscriptName;
    if (request.wants(Field::FacesPlatformShadowed)) seed.shadowed = shadowedFiles(group, seed.descriptor);

    std::string family = toUtf8(asString(copyAttribute(seed.descriptor, kCTFontFamilyNameAttribute).get()));
    const auto [entry, inserted] = familyOfName.try_emplace(family, families.size());
    if (inserted) families.push_back({std::move(family), {}});
    families[entry->second].seeds.push_back(std::move(seed));
  }
  return families;
}

// The descriptors CoreText's family lookup returns for `family`, in the order its matcher lists them, which is the
// order a browser walks them. Disabled fonts are included. Null when CoreText cannot make the lookup.
Ref<CFArrayRef> lookUpFamily(const std::string& family) {
  const auto familyName = makeString(family);
  const void* attributeKeys[] = {kCTFontFamilyNameAttribute};
  const void* attributeValues[] = {familyName.get()};
  const Ref<CFDictionaryRef> attributes(CFDictionaryCreate(kCFAllocatorDefault, attributeKeys, attributeValues, 1,
                                                           &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks));
  const Ref<CTFontDescriptorRef> descriptor(CTFontDescriptorCreateWithAttributes(attributes.get()));
  const void* descriptorList[] = {descriptor.get()};
  const Ref<CFArrayRef> descriptors(CFArrayCreate(kCFAllocatorDefault, descriptorList, 1, &kCFTypeArrayCallBacks));
  const Ref<CFDictionaryRef> options = collectionOptions();
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
  if (const Ref<CFArrayRef> members = lookUpFamily(family.name)) {
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
std::vector<FamilySeeds> familiesOfCollection(const Request& request, Ref<CFArrayRef>& collection) {
  collection = copyCollectionDescriptors();
  return groupByFamily(groupByPostscriptName(collection.get()), request);
}

// The faces of each requested family, from the family lookup. The groups come out in the lookup's order, so the
// family's faces are in member order already; the faces the full collection would add after them (those the lookup
// does not name) are not looked for. The lookup lists one file for a PostScript name, and the other files
// (`platform.shadowed`) only the collection knows, so a request for them also lists the collection and takes the
// group of each name from it. `collection` and `lookups` keep the descriptors alive.
std::vector<FamilySeeds> familiesOfLookups(const Request& request, Ref<CFArrayRef>& collection,
                                           std::vector<Ref<CFArrayRef>>& lookups) {
  std::vector<Group> collectionGroups;
  std::unordered_map<std::string, size_t> collectionGroupOfName;
  if (request.wants(Field::FacesPlatformShadowed)) {
    collection = copyCollectionDescriptors();
    collectionGroups = groupByPostscriptName(collection.get());
    for (size_t i = 0; i < collectionGroups.size(); ++i) {
      if (!collectionGroups[i].postscriptName.empty())
        collectionGroupOfName.emplace(collectionGroups[i].postscriptName, i);
    }
  }

  std::vector<FamilySeeds> result;
  std::vector<std::string> looked;
  for (const std::string& name : *request.families) {
    if (std::find(looked.begin(), looked.end(), name) != looked.end()) continue;
    looked.push_back(name);

    Ref<CFArrayRef> members = lookUpFamily(name);
    if (!members) continue;
    std::vector<Group> groups = groupByPostscriptName(members.get());
    for (Group& group : groups) {
      const auto inCollection = collectionGroupOfName.find(group.postscriptName);
      if (inCollection != collectionGroupOfName.end()) group = collectionGroups[inCollection->second];
    }
    // The lookup may return other families, and the same family may come out of the lookup of another requested
    // name: the family of this name is the one this lookup answers for.
    for (FamilySeeds& family : groupByFamily(groups, request)) {
      if (family.name != name) continue;
      family.ordered = true;
      result.push_back(std::move(family));
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
  // Keeps the descriptors the seeds point into alive.
  Ref<CFArrayRef> collection;
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
        family.faces.push_back(reader.read(seed.descriptor, seed.postscriptName, seed.shadowed));
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
