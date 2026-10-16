#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "sfnt.h"

// Bounds-checked big-endian access to a table's bytes. Every accessor takes an
// absolute offset and throws ParseError when the requested bytes are not
// inside the table, so a truncated or inconsistent table can never be read out
// of bounds.

namespace sfi::sfnt {

class Reader {
 public:
  explicit Reader(std::span<const uint8_t> data) : data_(data) {}

  size_t size() const { return data_.size(); }

  // Throws unless [offset, offset + length) lies inside the table.
  void require(size_t offset, size_t length) const {
    if (offset > data_.size() || length > data_.size() - offset) throw ParseError("table data out of bounds");
  }

  std::span<const uint8_t> bytes(size_t offset, size_t length) const {
    require(offset, length);
    return data_.subspan(offset, length);
  }

  uint8_t u8(size_t offset) const {
    require(offset, 1);
    return data_[offset];
  }
  uint16_t u16(size_t offset) const {
    require(offset, 2);
    return uint16_t((data_[offset] << 8) | data_[offset + 1]);
  }
  int16_t i16(size_t offset) const { return int16_t(u16(offset)); }
  uint32_t u32(size_t offset) const {
    require(offset, 4);
    return (uint32_t(data_[offset]) << 24) | (uint32_t(data_[offset + 1]) << 16) | (uint32_t(data_[offset + 2]) << 8) |
           uint32_t(data_[offset + 3]);
  }
  int32_t i32(size_t offset) const { return int32_t(u32(offset)); }
  uint64_t u64(size_t offset) const { return (uint64_t(u32(offset)) << 32) | u32(offset + 4); }
  int64_t i64(size_t offset) const { return int64_t(u64(offset)); }

  // Fixed 16.16.
  double fixed(size_t offset) const { return i32(offset) / 65536.0; }

 private:
  std::span<const uint8_t> data_;
};

}  // namespace sfi::sfnt
