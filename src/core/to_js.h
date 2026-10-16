#pragma once

#include <napi.h>

#include <vector>

#include "model.h"
#include "value.h"

namespace sfi {

// Converts an enumeration result to the `RawFamily[]` value the JS layer
// normalizes (see ts/native.ts). One conversion shares objects: every face that
// references the same `Value::Shared` subtree or the same `SharedAxes` gets the
// one JS object back. Call on the JS thread.
Napi::Array familiesToJs(Napi::Env env, const std::vector<Family>& families);

// Converts one shared subtree (for example a face's `opentype`) to JS.
Napi::Value sharedToJs(Napi::Env env, const Value::Shared& shared);

}  // namespace sfi
