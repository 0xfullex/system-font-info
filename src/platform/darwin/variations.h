#pragma once

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../core/model.h"
#include "../../sfnt/sfnt.h"
#include "cf.h"

namespace sfi::platform::darwin {

using Coordinates = std::vector<std::pair<std::string, double>>;

// The font's variation axes (kCTFontVariationAxesAttribute) with the names the
// `fvar` table gives each axis. Null when the font has no axes.
SharedAxes makeAxes(CFArrayRef coreTextAxes, const sfnt::FontTables& tables);

// The face's value on every axis, in axis order. CoreText reports only the
// axes that differ from their default (none at all for a default instance), so
// the others are filled in from the axis defaults.
Coordinates coordinatesOf(CTFontRef font, const std::vector<VariationAxis>& axes);

// Index of the first `fvar` instance whose coordinates equal the face's.
std::optional<uint32_t> namedInstanceOf(const Coordinates& coordinates, const std::optional<sfnt::Fvar>& fvar);

}  // namespace sfi::platform::darwin
