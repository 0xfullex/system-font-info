#include <algorithm>
#include <map>
#include <stdexcept>
#include <unordered_map>

#include "../../core/platform.h"
#include "appkit.h"
#include "cf.h"
#include "face_reader.h"

// enumerateFonts: the installed fonts as CoreText's font collection lists them.
//
// The collection lists a font once for each file that provides it, so a
// PostScript name can appear more than once (a system update ships a new
// version of a font next to the old one). CSS local(), font family matching and
// every API that takes a PostScript name resolve it to exactly one of them, so
// the result holds one face per PostScript name: the one the name resolves to.
// The others are not dropped silently: they are reported under
// `platform.shadowed` of that face.

namespace sfi::platform {
namespace {

using namespace darwin;

// All collection descriptors that share one PostScript name.
struct Group {
  std::string postscriptName;
  std::vector<CTFontDescriptorRef> descriptors;  // owned by the collection's array
};

Ref<CFArrayRef> copyCollectionDescriptors() {
  const void* keys[] = {kCTFontCollectionIncludeDisabledFontsOption};
  const void* values[] = {kCFBooleanTrue};
  const Ref<CFDictionaryRef> options(CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1, &kCFTypeDictionaryKeyCallBacks,
                                                        &kCFTypeDictionaryValueCallBacks));
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

bool isVariable(CTFontDescriptorRef descriptor) {
  CFArrayRef axes = asArray(copyAttribute(descriptor, kCTFontVariationAxesAttribute).get());
  return axes && CFArrayGetCount(axes) > 0;
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
    if (resolvedStorage && postscriptNameOf(resolvedStorage.get()) == group.postscriptName) return resolvedStorage.get();
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
  std::vector<Seed> seeds;  // in collection order
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
    shadowed.push_back({std::move(file), isVariable(other)});
  }
  return shadowed;
}

std::vector<FamilySeeds> groupByFamily(const std::vector<Group>& groups) {
  std::vector<FamilySeeds> families;
  std::unordered_map<std::string, size_t> familyOfName;
  for (const Group& group : groups) {
    Seed seed;
    seed.descriptor = resolve(group, seed.resolved);
    seed.postscriptName = group.postscriptName;
    seed.shadowed = shadowedFiles(group, seed.descriptor);

    std::string family = toUtf8(asString(copyAttribute(seed.descriptor, kCTFontFamilyNameAttribute).get()));
    const auto [entry, inserted] = familyOfName.try_emplace(family, families.size());
    if (inserted) families.push_back({std::move(family), {}});
    families[entry->second].seeds.push_back(std::move(seed));
  }
  return families;
}

// NSFontManager's order of the family's members first, then the faces it does
// not list, in collection order.
void orderLikeAppKit(FamilySeeds& family, const std::vector<AppKitMember>& members,
                     std::vector<const AppKitMember*>& memberOfSeed) {
  std::vector<size_t> order;
  std::vector<bool> placed(family.seeds.size(), false);
  memberOfSeed.assign(family.seeds.size(), nullptr);
  for (const AppKitMember& member : members) {
    for (size_t i = 0; i < family.seeds.size(); ++i) {
      if (placed[i] || family.seeds[i].postscriptName != member.postscriptName) continue;
      placed[i] = true;
      memberOfSeed[i] = &member;
      order.push_back(i);
      break;
    }
  }
  for (size_t i = 0; i < family.seeds.size(); ++i) {
    if (!placed[i]) order.push_back(i);
  }

  std::vector<Seed> ordered;
  std::vector<const AppKitMember*> orderedMembers;
  for (size_t i : order) {
    ordered.push_back(std::move(family.seeds[i]));
    orderedMembers.push_back(memberOfSeed[i]);
  }
  family.seeds = std::move(ordered);
  memberOfSeed = std::move(orderedMembers);
}

std::vector<Family> enumerate(const EnumerateOptions& options) {
  const Ref<CFArrayRef> collection = copyCollectionDescriptors();
  std::vector<FamilySeeds> families = groupByFamily(groupByPostscriptName(collection.get()));

  FaceReader reader(options);
  std::vector<Family> result;
  result.reserve(families.size());
  for (FamilySeeds& seeds : families) {
    const std::vector<AppKitMember> members = appKitFamilyMembers(seeds.name);
    std::vector<const AppKitMember*> memberOfSeed;
    orderLikeAppKit(seeds, members, memberOfSeed);

    Family family;
    family.family = seeds.name;
    for (size_t i = 0; i < seeds.seeds.size(); ++i) {
      family.faces.push_back(reader.read(seeds.seeds[i].descriptor, seeds.seeds[i].shadowed, memberOfSeed[i]));
    }
    result.push_back(std::move(family));
  }
  return result;
}

}  // namespace

std::vector<Family> enumerateFonts(const EnumerateOptions& options) {
  std::vector<Family> result;
  runInAutoreleasePool([&] { result = enumerate(options); });
  return result;
}

}  // namespace sfi::platform
