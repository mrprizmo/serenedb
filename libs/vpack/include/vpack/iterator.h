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

#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <tuple>

#include "vpack/common.h"
#include "vpack/exception.h"
#include "vpack/slice.h"
#include "vpack/value_type.h"

namespace vpack {

class ArrayIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = ptrdiff_t;
  using value_type = Slice;
  using pointer = Slice*;
  using reference = Slice&;

  struct Empty {};

  ArrayIterator() = delete;

  // optimization for an empty array
  explicit ArrayIterator(Empty) noexcept : _slice{Slice::emptyArraySlice()} {}

  explicit ArrayIterator(Slice slice) : _slice{slice} {
    if (!slice.isArray()) [[unlikely]] {
      throw Exception{Exception::kInvalidValueType, "Expecting Array slice"};
    }
    _size = slice.arrayLength();
    _current = first();
  }

  ArrayIterator& operator++() noexcept {
    next();
    return *this;
  }

  [[nodiscard]] ArrayIterator operator++(int) noexcept {
    auto prev = *this;
    next();
    return prev;
  }

  [[nodiscard]] Slice operator*() const { return value(); }

  [[nodiscard]] ArrayIterator begin() const noexcept { return {*this}; }
  [[nodiscard]] std::default_sentinel_t end() const noexcept { return {}; }

  [[nodiscard]] bool operator==(std::default_sentinel_t) const noexcept {
    return !valid();
  }

  [[nodiscard]] bool valid() const noexcept { return _position != _size; }

  [[nodiscard]] Slice value() const {
    if (!valid()) [[unlikely]] {
      throw Exception{Exception::kIndexOutOfBounds};
    }
    SDB_ASSERT(_current);
    return Slice{_current};
  }

  void next() noexcept {
    ++_position;
    if (_position >= _size) [[unlikely]] {
      _position = _size;
      return;
    }
    SDB_ASSERT(_current);
    _current += Slice{_current}.byteSize();
  }

  [[nodiscard]] ValueLength index() const noexcept { return _position; }

  [[nodiscard]] ValueLength size() const noexcept { return _size; }

  void forward(ValueLength count) noexcept {
    _position += count;
    if (_position >= _size) [[unlikely]] {
      _position = _size;
      return;
    }
    if (_slice.head() == 0x13) {
      while (count-- != 0) {
        _current += Slice{_current}.byteSize();
      }
    } else {
      _current = _slice.at(_position).start();
    }
  }

  void reset() noexcept {
    _position = 0;
    _current = first();
  }

 protected:
  [[nodiscard]] const uint8_t* first() const noexcept {
    if (_size == 0) [[unlikely]] {
      return nullptr;
    }
    const auto head = _slice.head();
    SDB_ASSERT(head != 0x01);  // no empty array allowed here
    if (head == 0x13) {
      return _slice.start() + _slice.getStartOffsetFromCompact();
    }
    return _slice.begin() + _slice.findDataOffset(head);
  }

  Slice _slice;
  const uint8_t* _current = nullptr;
  ValueLength _position = 0;
  ValueLength _size = 0;
};

struct ObjectIteratorKeyValue {
  Slice key;
  Slice value() const { return Slice{key.start() + key.byteSize()}; }

  template<size_t I>
  auto get() const {
    if constexpr (I == 0) {
      return key;
    } else if constexpr (I == 1) {
      return value();
    }
  }
};

class ObjectIterator {
 public:
  using iterator_category = std::forward_iterator_tag;
  using difference_type = ptrdiff_t;
  using value_type = ObjectIteratorKeyValue;
  using pointer = ObjectIteratorKeyValue*;
  using reference = ObjectIteratorKeyValue&;

  ObjectIterator() = delete;

  // The use_sequential_iteration flag indicates whether or not the iteration
  // simply jumps from key/value pair to key/value pair without using the
  // index. The default `false` is to use the index if it is there.
  explicit ObjectIterator(Slice slice, bool use_sequential_iteration = false)
    : _slice{slice} {
    if (!slice.isObject()) [[unlikely]] {
      throw Exception{Exception::kInvalidValueType, "Expecting Object slice"};
    }
    _size = slice.objectLength();
    _current = first(use_sequential_iteration);
  }

  [[nodiscard]] bool valid() const noexcept { return _position != _size; }

  void next() noexcept {
    ++_position;
    if (_position >= _size) [[unlikely]] {
      _position = _size;
      return;
    }
    if (_current) {
      _current += Slice{_current}.byteSize();  // skip over key
      _current += Slice{_current}.byteSize();  // skip over value
    }
  }

  ObjectIterator& operator++() noexcept {
    next();
    return *this;
  }

  [[nodiscard]] ObjectIterator operator++(int) noexcept {
    auto prev = *this;
    next();
    return prev;
  }

  [[nodiscard]] ObjectIteratorKeyValue operator*() const {
    if (_current) {
      if (!valid()) [[unlikely]] {
        throw Exception{Exception::kIndexOutOfBounds};
      }
      return {Slice{_current}};
    }
    // intentionally no out-of-bounds checking here,
    // as it will be performed by Slice::getNthOffset()
    return {_slice.getNthKeyUntranslated(_position)};
  }

  [[nodiscard]] ObjectIterator begin() const noexcept { return {*this}; }
  [[nodiscard]] std::default_sentinel_t end() const noexcept { return {}; }

  [[nodiscard]] bool operator==(std::default_sentinel_t) const noexcept {
    return !valid();
  }

  [[nodiscard]] ValueLength index() const noexcept { return _position; }

  [[nodiscard]] ValueLength size() const noexcept { return _size; }

  void reset() noexcept {
    _position = 0;
    _current = first(_current != nullptr);
  }

 protected:
  [[nodiscard]] const uint8_t* first(
    bool use_sequential_iteration) const noexcept {
    if (_size == 0) [[unlikely]] {
      return nullptr;
    }
    const auto head = _slice.head();
    SDB_ASSERT(head != 0x0a);  // no empty object allowed here
    if (head == 0x14) {
      return _slice.start() + _slice.getStartOffsetFromCompact();
    } else if (use_sequential_iteration) {
      return _slice.start() + _slice.findDataOffset(head);
    }
    return nullptr;
  }

  Slice _slice;
  const uint8_t* _current = nullptr;
  ValueLength _size = 0;
  ValueLength _position = 0;
};

struct IteratorValue {
  static_assert(std::is_unsigned_v<ValueLength>);

  vpack::ValueType type = vpack::ValueType::None;
  vpack::ValueLength pos = -1;
  vpack::Slice key;
  vpack::Slice value;
};

class Iterator {
 public:
  explicit Iterator(vpack::Slice slice) : _length{slice.length()} {
    if (_length == 0) {
      return;
    }

    const auto head = slice.head();
    const auto offset = head == 0x13 || head == 0x14
                          ? slice.getStartOffsetFromCompact()
                          : slice.findDataOffset(head);
    _begin = slice.start() + offset;
    _value.type = slice.type();
  }

  // returns true if iterator exhausted
  bool next() noexcept {
    if (0 == _length) {
      return false;
    }

    // whether or not we're in the context of array or object
    const auto is_object = _value.type != ValueType::Array;

    _value.key = Slice{_begin};
    _value.value = Slice{_begin + (is_object ? _value.key.byteSize() : 0)};

    _begin = _value.value.start() + _value.value.byteSize();
    ++_value.pos;
    --_length;

    return true;
  }

  bool valid() const noexcept { return 0 != _length; }

  const IteratorValue& operator*() const noexcept { return _value; }

 private:
  vpack::ValueLength _length;
  const uint8_t* _begin = nullptr;
  IteratorValue _value;
};

inline size_t operator-(std::default_sentinel_t, const ArrayIterator& it) {
  return it.size();
}

inline size_t operator-(std::default_sentinel_t, const ObjectIterator& it) {
  return it.size();
}

}  // namespace vpack

std::ostream& operator<<(std::ostream&, const vpack::ArrayIterator*);

std::ostream& operator<<(std::ostream&, const vpack::ArrayIterator&);

std::ostream& operator<<(std::ostream&, const vpack::ObjectIterator*);

std::ostream& operator<<(std::ostream&, const vpack::ObjectIterator&);

template<>
struct std::tuple_size<vpack::ObjectIteratorKeyValue>
  : integral_constant<size_t, 2> {};

template<size_t I>
struct std::tuple_element<I, vpack::ObjectIteratorKeyValue> {
  using type = vpack::Slice;
};
