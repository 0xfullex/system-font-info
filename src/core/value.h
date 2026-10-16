#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace sfi {

// A JSON-like tree for the open-ended parts of a face (`platform`, `opentype`).
// The fixed, cross-platform fields live in typed structs (model.h); this type
// carries data whose shape is defined per table or per platform.
//
// A `Shared` node references a subtree owned elsewhere. The JS conversion
// creates one JS object per shared subtree, so faces that point at the same
// subtree (named instances of one font resource) share one object in JS too.
class Value {
 public:
  using Array = std::vector<Value>;
  using Object = std::vector<std::pair<std::string, Value>>;  // insertion order is kept
  using Shared = std::shared_ptr<const Value>;

  Value() = default;  // null
  Value(bool b) : data_(b) {}
  template <typename T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, int> = 0>
  Value(T number) : data_(static_cast<double>(number)) {}
  Value(std::string s) : data_(std::move(s)) {}
  Value(const char* s) : data_(std::string(s)) {}
  Value(Array a) : data_(std::move(a)) {}
  Value(Object o) : data_(std::move(o)) {}
  Value(Shared s) : data_(std::move(s)) {}

  static Value object() { return Value(Object{}); }
  static Value array() { return Value(Array{}); }

  // Appends a key to an object. Keys are not deduplicated; callers set each key once.
  Value& set(std::string key, Value v) {
    std::get<Object>(data_).emplace_back(std::move(key), std::move(v));
    return *this;
  }
  // Appends an element to an array.
  Value& push(Value v) {
    std::get<Array>(data_).push_back(std::move(v));
    return *this;
  }

  bool isNull() const { return std::holds_alternative<std::monostate>(data_); }

  using Data = std::variant<std::monostate, bool, double, std::string, Array, Object, Shared>;
  const Data& data() const { return data_; }

 private:
  Data data_;
};

}  // namespace sfi
