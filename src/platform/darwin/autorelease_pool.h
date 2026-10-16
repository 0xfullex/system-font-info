#pragma once

#include <functional>

// The only Objective-C++ of the darwin backend (autorelease_pool.mm).
// Everything else is plain CoreText, so the rest of the backend stays C++.

namespace sfi::platform::darwin {

// Runs `body` inside an Objective-C autorelease pool. CoreText autoreleases
// objects internally (the runtime reports them as "autoreleased with no pool"
// on a thread that has none); a worker thread has no pool of its own, so
// without one those objects would leak. An exception thrown by `body`
// propagates to the caller after the pool is drained.
void runInAutoreleasePool(const std::function<void()>& body);

}  // namespace sfi::platform::darwin
