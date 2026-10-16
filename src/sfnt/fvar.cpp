#include "internal.h"
#include "reader.h"

namespace sfi::sfnt {

namespace {

constexpr size_t kMinAxisSize = 20;
constexpr size_t kInstanceFixedSize = 4;  // subfamilyNameID + flags, before the coordinates
constexpr uint16_t kNoPostscriptName = 0xFFFF;

}  // namespace

Fvar parseFvar(std::span<const uint8_t> table) {
  const Reader r(table);
  if (r.u16(0) != 1) throw ParseError("unknown fvar version");
  const size_t axesOffset = r.u16(4);
  const size_t axisCount = r.u16(8);
  const size_t axisSize = r.u16(10);
  const size_t instanceCount = r.u16(12);
  const size_t instanceSize = r.u16(14);
  if (axisSize < kMinAxisSize) throw ParseError("fvar axis record too small");
  const size_t coordinatesSize = axisCount * 4;
  if (instanceSize < kInstanceFixedSize + coordinatesSize) throw ParseError("fvar instance record too small");

  const size_t instancesOffset = axesOffset + axisCount * axisSize;
  r.require(axesOffset, axisCount * axisSize);
  r.require(instancesOffset, instanceCount * instanceSize);

  Fvar fvar;
  for (size_t i = 0; i < axisCount; ++i) {
    const size_t at = axesOffset + i * axisSize;
    FvarAxis axis;
    axis.tag = r.u32(at);
    axis.min = r.fixed(at + 4);
    axis.defaultValue = r.fixed(at + 8);
    axis.max = r.fixed(at + 12);
    axis.flags = r.u16(at + 16);
    axis.nameId = r.u16(at + 18);
    fvar.axes.push_back(axis);
  }

  // The PostScript name ID is stored only when the record has room for exactly it.
  const bool hasPostscriptNameId = instanceSize == kInstanceFixedSize + coordinatesSize + 2;
  for (size_t i = 0; i < instanceCount; ++i) {
    const size_t at = instancesOffset + i * instanceSize;
    FvarInstance instance;
    instance.subfamilyNameId = r.u16(at);
    instance.flags = r.u16(at + 2);
    for (size_t axis = 0; axis < axisCount; ++axis) instance.coordinates.push_back(r.fixed(at + 4 + axis * 4));
    if (hasPostscriptNameId) {
      const uint16_t id = r.u16(at + 4 + coordinatesSize);
      if (id != kNoPostscriptName) instance.postscriptNameId = id;
    }
    fvar.instances.push_back(std::move(instance));
  }
  return fvar;
}

Value fvarToValue(const Fvar& fvar, const std::vector<NameRecord>& names) {
  Value axes = Value::array();
  for (const FvarAxis& axis : fvar.axes) {
    Value item = Value::object();
    item.set("tag", tagToString(axis.tag))
        .set("min", axis.min)
        .set("default", axis.defaultValue)
        .set("max", axis.max)
        .set("flags", axis.flags)
        .set("nameId", axis.nameId)
        .set("names", localizedStringsToValue(localizedName(names, axis.nameId)));
    axes.push(std::move(item));
  }

  Value instances = Value::array();
  for (const FvarInstance& instance : fvar.instances) {
    Value coordinates = Value::object();
    for (size_t i = 0; i < fvar.axes.size(); ++i) {
      coordinates.set(tagToString(fvar.axes[i].tag), instance.coordinates[i]);
    }
    Value item = Value::object();
    item.set("subfamilyNameId", instance.subfamilyNameId)
        .set("subfamilyNames", localizedStringsToValue(localizedName(names, instance.subfamilyNameId)))
        .set("flags", instance.flags)
        .set("coordinates", std::move(coordinates));
    if (instance.postscriptNameId) {
      item.set("postscriptNameId", *instance.postscriptNameId);
      // The first decoded record of that nameID, whatever its language.
      for (const NameRecord& record : names) {
        if (record.nameId == *instance.postscriptNameId && record.value) {
          item.set("postscriptName", *record.value);
          break;
        }
      }
    }
    instances.push(std::move(item));
  }

  Value out = Value::object();
  out.set("axes", std::move(axes)).set("instances", std::move(instances));
  return out;
}

}  // namespace sfi::sfnt
