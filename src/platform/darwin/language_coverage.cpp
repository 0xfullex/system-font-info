#include "language_coverage.h"

namespace sfi::platform::darwin {

std::string matchDefaultFaceName(const std::string& family) {
  const auto name = makeString(family);
  const double zero = 0;
  const Ref<CFNumberRef> weight(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &zero));
  const Ref<CFNumberRef> width(CFNumberCreate(kCFAllocatorDefault, kCFNumberDoubleType, &zero));
  const void* traitKeys[] = {kCTFontWeightTrait, kCTFontWidthTrait};
  const void* traitValues[] = {weight.get(), width.get()};
  const Ref<CFDictionaryRef> traits(CFDictionaryCreate(kCFAllocatorDefault, traitKeys, traitValues, 2,
                                                       &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks));
  const void* keys[] = {kCTFontFamilyNameAttribute, kCTFontTraitsAttribute};
  const void* values[] = {name.get(), traits.get()};
  const Ref<CFDictionaryRef> attributes(CFDictionaryCreate(
      kCFAllocatorDefault, keys, values, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  const Ref<CTFontDescriptorRef> request(CTFontDescriptorCreateWithAttributes(attributes.get()));

  // Without the mandatory family the matcher falls back to other families when it finds none.
  const void* mandatoryKeys[] = {kCTFontFamilyNameAttribute};
  const Ref<CFSetRef> mandatory(CFSetCreate(kCFAllocatorDefault, mandatoryKeys, 1, &kCFTypeSetCallBacks));
  const Ref<CTFontDescriptorRef> match(CTFontDescriptorCreateMatchingFontDescriptor(request.get(), mandatory.get()));
  if (!match || toUtf8(asString(copyAttribute(match.get(), kCTFontFamilyNameAttribute).get())) != family) return {};
  return toUtf8(asString(copyAttribute(match.get(), kCTFontNameAttribute).get()));
}

std::vector<uint32_t> countLanguageCoverage(CTFontDescriptorRef face,
                                            const std::vector<const orthography::Orthography*>& orthographies) {
  std::vector<uint32_t> covered(orthographies.size(), 0);
  const auto attribute = copyAttribute(face, kCTFontCharacterSetAttribute);
  if (!attribute || CFGetTypeID(attribute.get()) != CFCharacterSetGetTypeID()) return covered;
  const auto characterSet = static_cast<CFCharacterSetRef>(attribute.get());
  for (size_t i = 0; i < orthographies.size(); ++i) {
    covered[i] = orthography::countCovered(*orthographies[i], [&](uint32_t codePoint) {
      return CFCharacterSetIsLongCharacterMember(characterSet, codePoint);
    });
  }
  return covered;
}

}  // namespace sfi::platform::darwin
