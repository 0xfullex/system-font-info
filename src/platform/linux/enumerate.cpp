#include <map>
#include <set>

#include "../../core/platform.h"
#include "face_reader.h"
#include "fc.h"
#include "testing.h"

namespace sfi::platform {
namespace {
using namespace linux_backend;

ObjectSet objects(const Request& request) {
  ObjectSet result(FcObjectSetCreate());
  require(bool(result), "FcObjectSetCreate");
  const char* scalar[] = {
      FC_FAMILY, FC_FAMILYLANG, FC_FULLNAME,   FC_FULLNAMELANG, FC_STYLE,     FC_STYLELANG,  FC_POSTSCRIPT_NAME,
      FC_WEIGHT, FC_WIDTH,      FC_SLANT,      FC_SPACING,      FC_COLOR,     FC_SYMBOL,     FC_FILE,
      FC_INDEX,  FC_VARIABLE,   FC_FONTFORMAT, FC_FONT_WRAPPER, FC_FOUNDRY,   FC_DECORATIVE, FC_FONT_HAS_HINT,
      FC_ORDER,  FC_OUTLINE,    FC_SCALABLE,   FC_ANTIALIAS,    FC_PIXEL_SIZE};
  for (const char* object : scalar) require(FcObjectSetAdd(result.get(), object), "FcObjectSetAdd");
  if (request.needs(Group::E) || !request.orthographies.empty())
    require(FcObjectSetAdd(result.get(), FC_CHARSET), "adding FC_CHARSET");
  if (request.wants(Field::FacesSupportedLanguages)) require(FcObjectSetAdd(result.get(), FC_LANG), "adding FC_LANG");
  return result;
}

Pattern query(const std::string* family = nullptr) {
  Pattern result(FcPatternCreate());
  require(bool(result), "FcPatternCreate");
  require(FcPatternAddString(result.get(), FC_NAMELANG, reinterpret_cast<const FcChar8*>("en")), "adding FC_NAMELANG");
  if (family)
    require(FcPatternAddString(result.get(), FC_FAMILY, reinterpret_cast<const FcChar8*>(family->c_str())),
            "adding FC_FAMILY");
  return result;
}

void languageCoverage(FcConfig* config, const std::string& family, const std::vector<FcPattern*>& originals,
                      const Request& request, WorkLog& work, Family& result) {
  auto matchQuery = query(&family);
  require(FcPatternAddInteger(matchQuery.get(), FC_WEIGHT, FC_WEIGHT_REGULAR), "adding FC_WEIGHT");
  require(FcPatternAddInteger(matchQuery.get(), FC_WIDTH, FC_WIDTH_NORMAL), "adding FC_WIDTH");
  require(FcPatternAddInteger(matchQuery.get(), FC_SLANT, FC_SLANT_ROMAN), "adding FC_SLANT");
  require(FcConfigSubstitute(config, matchQuery.get(), FcMatchPattern), "FcConfigSubstitute");
  // FcConfigSubstitute adds a weak language taken from FC_LANG, LC_ALL, LC_CTYPE or LANG, even to a query that has
  // one, and the matcher ranks the language before slant, weight and width: the default face would depend on the
  // process's locale. The query's language is replaced by a fixed one; FcDefaultSubstitute then adds none.
  FcPatternDel(matchQuery.get(), FC_LANG);
  require(FcPatternAddString(matchQuery.get(), FC_LANG, reinterpret_cast<const FcChar8*>("en")), "adding FC_LANG");
  FcDefaultSubstitute(matchQuery.get());
  FontSet members(FcFontSetCreate());
  require(bool(members), "FcFontSetCreate");
  for (auto original : originals) {
    FcPatternReference(original);
    if (!FcFontSetAdd(members.get(), original)) {
      FcPatternDestroy(original);
      require(false, "FcFontSetAdd");
    }
  }
  FcFontSet* sets[] = {members.get()};
  FcResult status;
  Pattern matched(FcFontSetMatch(config, sets, 1, matchQuery.get(), &status));
  // The set contains this family's fixed patterns only, so aliases cannot select another family, and a set that is
  // not empty matches unless memory runs out.
  require(bool(matched), "FcFontSetMatch");
  FcCharSet* characters = charset(matched.get());
  ++work.characterSets;
  for (auto orthography : request.orthographies) {
    result.languageCoverage.push_back(orthography::countCovered(
        *orthography, [&](uint32_t codePoint) { return FcCharSetHasChar(characters, codePoint) != FcFalse; }));
  }
}
}  // namespace

std::vector<Family> enumerateFonts(const Request& request, WorkLog& work) {
  if (request.families && request.families->empty()) return {};
  const auto turn = serializeOnOldFontconfig();
  Config config(FcInitLoadConfigAndFonts());
  require(bool(config), "FcInitLoadConfigAndFonts");
  addTestingFonts(config.get());
  auto objectSet = objects(request);
  std::vector<FontSet> lists;
  if (request.families) {
    const std::set<std::string> names(request.families->begin(), request.families->end());
    for (const auto& name : names) {
      auto pattern = query(&name);
      lists.emplace_back(FcFontList(config.get(), pattern.get(), objectSet.get()));
      require(bool(lists.back()), "FcFontList");
    }
  } else {
    auto pattern = query();
    lists.emplace_back(FcFontList(config.get(), pattern.get(), objectSet.get()));
    require(bool(lists.back()), "FcFontList");
  }
  std::map<Identity, FcPattern*> candidates;
  for (const auto& list : lists) {
    for (int i = 0; i < list->nfont; ++i) {
      auto pattern = list->fonts[i];
      if (boolean(pattern, FC_VARIABLE).value_or(false)) continue;
      auto family = string(pattern, FC_FAMILY);
      require(family.has_value(), "reading FC_FAMILY");
      if (request.wantsFamily(*family)) candidates.try_emplace(identity(pattern), pattern);
    }
  }
  // FcFontList hashes the selected values. Restore the config's order before grouping,
  // so adding LANG or CHARSET changes neither face order nor the order of their names.
  struct Member {
    FcPattern* listed;
    FcPattern* original;
    bool inApplicationSet;
  };
  std::vector<Member> ordered;
  std::set<Identity> seen;
  std::set<Identity> variableResources;
  for (FcSetName which : {FcSetSystem, FcSetApplication}) {
    FcFontSet* fonts = FcConfigGetFonts(config.get(), which);
    if (!fonts) continue;
    for (int i = 0; i < fonts->nfont; ++i) {
      auto original = fonts->fonts[i];
      auto id = identity(original);
      if (boolean(original, FC_VARIABLE).value_or(false)) {
        variableResources.insert(resource(id));
      } else if (auto found = candidates.find(id); found != candidates.end() && seen.insert(id).second) {
        ordered.push_back({found->second, original, which == FcSetApplication});
      }
    }
  }
  require(seen.size() == candidates.size(), "locating listed fonts in config sets");
  std::vector<FcPattern*> listedPatterns;
  for (const auto& member : ordered) listedPatterns.push_back(member.listed);
  FaceReader reader(request, work, variableResources, listedPatterns);
  std::vector<Family> result;
  std::map<std::string, size_t> familyPositions;
  std::vector<std::vector<FcPattern*>> originalMembers;
  for (const auto& member : ordered) {
    const std::string family = *string(member.listed, FC_FAMILY);
    auto [position, added] = familyPositions.try_emplace(family, result.size());
    if (added) {
      result.emplace_back();
      result.back().family = family;
      originalMembers.emplace_back();
    }
    const size_t index = position->second;
    originalMembers[index].push_back(member.original);
    if (request.wantsAnyFace() && reader.lists(member.listed))
      result[index].faces.push_back(reader.read(member.listed, member.inApplicationSet));
  }
  if (!request.orthographies.empty()) {
    for (size_t i = 0; i < result.size(); ++i)
      languageCoverage(config.get(), result[i].family, originalMembers[i], request, work, result[i]);
  }
  return result;
}
}  // namespace sfi::platform
