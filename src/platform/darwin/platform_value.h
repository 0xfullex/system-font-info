#pragma once

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
  CTFontRef font;
  const Traits& traits;
  const AppKitMember* appKitMember;  // null when AppKit does not list the face
  bool withMetrics;
  const std::vector<ShadowedFile>& shadowed;
};

Value makePlatformValue(const PlatformValueInput& input);

}  // namespace sfi::platform::darwin
