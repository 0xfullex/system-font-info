#include "coverage.h"

namespace sfi::platform::darwin {
namespace {

constexpr size_t kPlaneBytes = 8192;  // one bit for each of a plane's 65536 code points

using Ranges = std::vector<std::pair<uint32_t, uint32_t>>;

// Adds a range, joining it to the previous one when they touch (a run that
// crosses a plane boundary arrives as two runs).
void addRange(Ranges& ranges, uint32_t first, uint32_t last) {
  if (!ranges.empty() && ranges.back().second + 1 == first) {
    ranges.back().second = last;
  } else {
    ranges.emplace_back(first, last);
  }
}

// Appends the ranges of one plane bitmap. Bit `n % 8` of byte `n / 8` is code
// point `n` of the plane, the layout of CFCharacterSetIsLongCharacterMember.
void appendPlane(const uint8_t* bitmap, uint32_t planeBase, Ranges& ranges) {
  bool open = false;
  uint32_t start = 0;
  for (size_t byteIndex = 0; byteIndex < kPlaneBytes; ++byteIndex) {
    const uint8_t byte = bitmap[byteIndex];
    if ((byte == 0 && !open) || (byte == 0xFF && open)) continue;  // no boundary inside this byte
    for (uint32_t bit = 0; bit < 8; ++bit) {
      const bool set = (byte >> bit) & 1;
      const uint32_t codePoint = planeBase + static_cast<uint32_t>(byteIndex) * 8 + bit;
      if (set && !open) {
        open = true;
        start = codePoint;
      } else if (!set && open) {
        open = false;
        addRange(ranges, start, codePoint - 1);
      }
    }
  }
  if (open) addRange(ranges, start, planeBase + 0xFFFF);
}

}  // namespace

std::vector<std::pair<uint32_t, uint32_t>> coverageRanges(CTFontDescriptorRef descriptor) {
  Ranges ranges;
  const auto attribute = copyAttribute(descriptor, kCTFontCharacterSetAttribute);
  if (!attribute || CFGetTypeID(attribute.get()) != CFCharacterSetGetTypeID()) return ranges;

  // The bitmap representation is the BMP's bitmap, then for each further
  // plane a plane number byte (1-16) followed by that plane's bitmap.
  const Ref<CFDataRef> data(
      CFCharacterSetCreateBitmapRepresentation(kCFAllocatorDefault, static_cast<CFCharacterSetRef>(attribute.get())));
  if (!data) return ranges;
  const uint8_t* bytes = CFDataGetBytePtr(data.get());
  const size_t length = static_cast<size_t>(CFDataGetLength(data.get()));

  if (length >= kPlaneBytes) appendPlane(bytes, 0, ranges);
  for (size_t offset = kPlaneBytes; offset + 1 + kPlaneBytes <= length; offset += 1 + kPlaneBytes) {
    appendPlane(bytes + offset + 1, static_cast<uint32_t>(bytes[offset]) << 16, ranges);
  }
  return ranges;
}

}  // namespace sfi::platform::darwin
