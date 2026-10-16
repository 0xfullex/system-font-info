#include "appkit.h"

#include <exception>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

namespace sfi::platform::darwin {

std::vector<AppKitMember> appKitFamilyMembers(const std::string& family) {
  std::vector<AppKitMember> members;
  @autoreleasepool {
    NSString* name = [NSString stringWithUTF8String:family.c_str()];
    if (!name) return members;
    // Each entry is [postscript name, style name, weight, traits].
    NSArray<NSArray*>* entries = [[NSFontManager sharedFontManager] availableMembersOfFontFamily:name];
    for (NSArray* entry in entries) {
      if (entry.count < 4) continue;
      AppKitMember member;
      member.postscriptName = [entry[0] UTF8String];
      member.weight = [entry[2] intValue];
      member.traits = [entry[3] unsignedLongValue];
      members.push_back(std::move(member));
    }
  }
  return members;
}

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
