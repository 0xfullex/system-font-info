#pragma once

#include <optional>
#include <vector>

#include "../../core/request.h"
#include "../../core/value.h"
#include "cf.h"
#include "face_reader.h"
#include "traits.h"

namespace sfi::platform::darwin {

// Everything that goes into `Face::platform` (the DarwinNative type in
// ts/types.ts). Only the keys the request wants are read and set; they keep
// that type's key order.
struct PlatformValueInput {
  const Request& request;
  CTFontDescriptorRef descriptor;
  CTFontRef font;         // for `metrics`; null when CoreText created another font than the descriptor names
  const Value* features;  // from readFeatures; null leaves `features` out
  const Traits& traits;   // read when the request wants a platform field that comes from the traits
  const std::vector<ShadowedFile>& shadowed;
};

// The features of the font with their names (CTFontCopyFeatures); nullopt when CoreText has none.
std::optional<Value> readFeatures(CTFontRef font);

Value makePlatformValue(const PlatformValueInput& input);

}  // namespace sfi::platform::darwin
