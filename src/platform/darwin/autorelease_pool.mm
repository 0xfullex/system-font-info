#include "autorelease_pool.h"

#include <exception>

#import <Foundation/Foundation.h>

namespace sfi::platform::darwin {

void runInAutoreleasePool(const std::function<void()>& body) {
  std::exception_ptr failure;
  @autoreleasepool {
    try {
      body();
    } catch (...) {
      failure = std::current_exception();
    }
  }
  if (failure) std::rethrow_exception(failure);
}

}  // namespace sfi::platform::darwin
