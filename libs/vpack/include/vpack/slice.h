////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <absl/hash/hash.h>

#include "vpack/slice_base.h"

namespace vpack {

// This class provides read only access to a VPack value, it is
// intentionally light-weight (only one pointer value), such that
// it can easily be used to traverse larger VPack values.

// A Slice does not own the VPack data it points to!
class Slice : public SliceBase<Slice> {
  friend class Builder;
  friend class ArrayIterator;
  friend class ObjectIterator;
  friend class ValueSlice;

  // ptr() is the pointer to the first byte of the value. It should always be
  // accessed through the start() method as that allows subclasses to adjust
  // the start position should it be necessary. For example, the ValueSlice
  // class uses this to make tags transparent and behaves as if they did not
  // exist, unless explicitly queried for.
  const uint8_t* _start;

 public:
  static constexpr uint8_t kNoneData = 0x00;
  static constexpr uint8_t kNullData = 0x18;
  static constexpr uint8_t kFalseData = 0x19;
  static constexpr uint8_t kTrueData = 0x1a;
  static constexpr uint8_t kZeroData = 0x36;
  static constexpr uint8_t kEmptyStringData = 0x80;
  static constexpr uint8_t kEmptyArrayData = 0x01;
  static constexpr uint8_t kEmptyObjectData = 0x0a;

  // constructor for an empty Value of type None
  constexpr Slice() noexcept : Slice{&kNoneData} {}

  // creates a Slice from a pointer to a uint8_t array
  explicit constexpr Slice(const uint8_t* start) noexcept : _start{start} {}

  // No destructor, does not take part in memory management

  // Set new memory position
  void set(const uint8_t* s) { _start = s; }

  static constexpr auto noneSlice() noexcept { return Slice{&kNoneData}; }

  static constexpr auto nullSlice() noexcept { return Slice{&kNullData}; }

  static constexpr auto falseSlice() noexcept { return Slice{&kFalseData}; }

  static constexpr auto trueSlice() noexcept { return Slice{&kTrueData}; }

  static constexpr auto boolSlice(bool value) noexcept {
    return value ? trueSlice() : falseSlice();
  }

  static constexpr auto zeroSlice() noexcept { return Slice{&kZeroData}; }

  static constexpr auto emptyStringSlice() noexcept {
    return Slice{&kEmptyStringData};
  }

  static constexpr auto emptyArraySlice() noexcept {
    return Slice{&kEmptyArrayData};
  }

  static constexpr auto emptyObjectSlice() noexcept {
    return Slice{&kEmptyObjectData};
  }

  constexpr const uint8_t* getDataPtr() const noexcept { return _start; }
};

static_assert(!std::is_polymorphic<Slice>::value,
              "Slice must not be polymorphic");
static_assert(!std::has_virtual_destructor<Slice>::value,
              "Slice must not have virtual dtor");

extern template struct SliceBase<Slice, Slice>;

std::ostream& operator<<(std::ostream&, Slice);

}  // namespace vpack
