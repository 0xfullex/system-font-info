#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// The only AppKit use of the darwin backend (appkit.mm). Everything else is
// plain CoreText, so the rest of the backend stays C++.

namespace sfi::platform::darwin {

// One entry of NSFontManager's availableMembersOfFontFamily.
struct AppKitMember {
  std::string postscriptName;
  int weight = 0;       // 0-15
  uint64_t traits = 0;  // NSFontTraitMask
};

// The members of a family in the order AppKit lists them (the order the system
// font panel shows, and the order CoreText's matcher returns them in). Safe to
// call from any thread: it was measured to return what the main thread
// returns. Empty when AppKit knows no such family.
std::vector<AppKitMember> appKitFamilyMembers(const std::string& family);

// Runs `body` inside an Objective-C autorelease pool. CoreText and AppKit
// autorelease objects internally; a worker thread has no pool of its own, so
// without one those objects would leak. An exception thrown by `body`
// propagates to the caller after the pool is drained.
void runInAutoreleasePool(const std::function<void()>& body);

}  // namespace sfi::platform::darwin
