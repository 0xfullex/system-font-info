#pragma once

#include <napi.h>

#include <vector>

#include "model.h"
#include "request.h"
#include "value.h"

namespace sfi {

// Converts an enumeration result to the `RawFamily[]` value the JS layer
// normalizes (see ts/native.ts). The one place that decides what is output:
// a key appears only when `request` wants its field, so whatever a backend
// computed beyond the request does not reach JS. The exception is the entries
// of a face's `opentype` object: it is output whole, so the backend must have
// built it from `request.openTypeSelection` (see platform.h). One conversion shares objects:
// every face that references the same `Value::Shared` subtree or the same
// `SharedAxes` gets the one JS object back. Call on the JS thread.
Napi::Array familiesToJs(Napi::Env env, const std::vector<Family>& families, const Request& request);

// Converts one shared subtree (for example a face's `opentype`) to JS.
Napi::Value sharedToJs(Napi::Env env, const Value::Shared& shared);

}  // namespace sfi
