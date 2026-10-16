#pragma once

#include <optional>
#include <vector>

#include "../../core/value.h"
#include "appkit.h"
#include "cf.h"
#include "face_reader.h"
#include "traits.h"

namespace sfi::platform::darwin {

// Everything that goes into `Face::platform` (the DarwinNative type in
// ts/types.ts), in that type's key order.
struct PlatformValueInput {
  CTFontDescriptorRef descriptor;
  CTFontRef font;  // null when CoreText created another font than the descriptor names
  const Value* features;  // from readFeatures; null leaves `features` out
  const Traits& traits;
  const AppKitMember* appKitMember;  // null when AppKit does not list the face
  bool withMetrics;
  const std::vector<ShadowedFile>& shadowed;
};

// The features of the font with their names (CTFontCopyFeatures); nullopt when CoreText has none.
std::optional<Value> readFeatures(CTFontRef font);

Value makePlatformValue(const PlatformValueInput& input);

}  // namespace sfi::platform::darwin
