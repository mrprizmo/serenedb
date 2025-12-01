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
///
////////////////////////////////////////////////////////////////////////////////

#include "vpack/builder.h"

#include <absl/container/flat_hash_set.h>

#include <array>
#include <memory>
#include <string_view>

#include "basics/sink.h"
#include "vpack/common.h"
#include "vpack/dumper.h"
#include "vpack/iterator.h"

namespace vpack {
namespace {

// checks whether a memmove operation is allowed to get rid of the padding
bool IsAllowedToMemmove(const Options* options, const uint8_t* start,
                        std::vector<index_t>::iterator index_start,
                        std::vector<index_t>::iterator index_end,
                        ValueLength offset_size) {
  SDB_ASSERT(offset_size == 1 || offset_size == 2);

  if (options->padding_behavior == Options::PaddingBehavior::kNoPadding ||
      (offset_size == 1 &&
       options->padding_behavior == Options::PaddingBehavior::kFlexible)) {
    const size_t distance = std::distance(index_start, index_end);
    const size_t n = (std::min)(size_t(8 - 2 * offset_size), distance);
    for (size_t i = 0; i < n; i++) {
      if (start[index_start[i]] == 0x00) {
        return false;
      }
    }
    return true;
  }

  return false;
}

uint8_t DetermineArrayType(bool need_index_table, ValueLength offset_size) {
  uint8_t type;
  // Now build the table:
  if (need_index_table) {
    type = 0x06;
  } else {  // no index table
    type = 0x02;
  }
  // Finally fix the byte width in the type byte:
  if (offset_size == 2) {
    type += 1;
  } else if (offset_size == 4) {
    type += 2;
  } else if (offset_size == 8) {
    type += 3;
  }
  return type;
}

constexpr ValueLength kLinearAttributeUniquenessCutoff = 4;

// struct used when sorting index tables for objects:
struct SortEntry {
  const uint8_t* key;
  uint32_t len;
  index_t offset;
};
// static_assert(sizeof(SortEntry) == 16);

// Find the actual bytes of the attribute name of the VPack value
// at position base, also determine the length len of the attribute.
// This takes into account the different possibilities for the format
// of attribute names:
const uint8_t* FindAttrName(const uint8_t* base, uint32_t& len) {
  const uint8_t b = *base;
  if (b == 0xff) [[unlikely]] {
    len = ReadIntegerFixed<uint32_t, 4>(base + 1);
    return base + 1 + 4;
  }
  SDB_ASSERT(0x80 <= b);
  SDB_ASSERT(b <= 0xfe);
  len = b - 0x80;
  return base + 1;
}

bool CheckAttributeUniquenessUnsortedBrute(ObjectIterator& it) {
  std::array<std::string_view, kLinearAttributeUniquenessCutoff> keys;

  do {
    auto kv = *it;
    std::string_view key = kv.key.stringView();
    ValueLength index = it.index();
    // compare with all other already looked-at keys
    for (ValueLength i = 0; i < index; ++i) {
      if (keys[i] == key) [[unlikely]] {
        return false;
      }
    }
    keys[index] = key;
    it.next();
  } while (it.valid());

  return true;
}

bool CheckAttributeUniquenessUnsortedSet(ObjectIterator& it) {
  absl::flat_hash_set<std::string_view> duplicate_keys;
  duplicate_keys.reserve(it.size());
  do {
    auto kv = *it;
    Slice key = kv.key;
    SDB_ASSERT(key.isString());
    if (!duplicate_keys.emplace(key.stringView()).second) [[unlikely]] {
      // identical key
      return false;
    }
    it.next();
  } while (it.valid());
  return true;
}

}  // namespace

Builder::Builder(const Options* options)
  : _buffer{std::make_shared<BufferUInt8>()},
    _start{_buffer->data()},
    options{options} {
  if (options == nullptr) [[unlikely]] {
    throw Exception(Exception::kInternalError, "Options cannot be a nullptr");
  }
}

Builder::Builder(std::shared_ptr<BufferUInt8> buffer, const Options* options)
  : _buffer{std::move(buffer)}, options{options} {
  if (!_buffer) [[unlikely]] {
    throw Exception(Exception::kInternalError, "Buffer cannot be a nullptr");
  }
  if (options == nullptr) [[unlikely]] {
    throw Exception(Exception::kInternalError, "Options cannot be a nullptr");
  }
  _start = _buffer->data();
  _pos = _buffer->size();
}

Builder::Builder(BufferUInt8& buffer, const Options* options)
  : _buffer{std::shared_ptr<BufferUInt8>{}, &buffer},
    _start{buffer.data()},
    _pos{static_cast<index_t>(buffer.size())},
    options{options} {
  if (options == nullptr) [[unlikely]] {
    throw Exception(Exception::kInternalError, "Options cannot be a nullptr");
  }
}

Builder::Builder(Slice slice, const Options* options) : Builder{options} {
  add(slice);
}

Builder::Builder(const Builder& other)
  : _pos{other._pos},
    _key_written{other._key_written},
    _stack{other._stack},
    _indexes{other._indexes},
    options{other.options} {
  if (IsOwning(other._buffer)) {
    _buffer = std::make_shared<BufferUInt8>(*other._buffer);
  } else {
    _buffer = other._buffer;
  }
  if (_buffer) {
    _start = _buffer->data();
  }
}

Builder& Builder::operator=(const Builder& other) {
  if (this == &other) [[unlikely]] {
    return *this;
  }
  if (IsOwning(other._buffer)) {
    _buffer = std::make_shared<BufferUInt8>(*other._buffer);
  } else {
    _buffer = other._buffer;
  }
  _start = _buffer ? _buffer->data() : nullptr;
  _pos = other._pos;
  _key_written = other._key_written;
  _stack = other._stack;
  _indexes = other._indexes;
  options = other.options;
  return *this;
}

Builder::Builder(Builder&& other) noexcept
  : _buffer{std::move(other._buffer)},
    _start{std::exchange(other._start, nullptr)},
    _pos{std::exchange(other._pos, 0)},
    _key_written{std::exchange(other._key_written, false)},
    _stack{std::move(other._stack)},
    _indexes{std::move(other._indexes)},
    options{other.options} {
  SDB_ASSERT(!other._buffer);
  SDB_ASSERT(other._stack.empty());
  SDB_ASSERT(other._indexes.empty());
}

Builder& Builder::operator=(Builder&& other) noexcept {
  if (this == &other) [[unlikely]] {
    return *this;
  }
  _buffer = std::move(other._buffer);
  _start = std::exchange(other._start, nullptr);
  _pos = std::exchange(other._pos, 0);
  _key_written = std::exchange(other._key_written, false);
  _stack = std::move(other._stack);
  _indexes = std::move(other._indexes);
  options = other.options;
  SDB_ASSERT(!other._buffer);
  SDB_ASSERT(other._stack.empty());
  SDB_ASSERT(other._indexes.empty());
  return *this;
}

void Builder::sortObjectIndexShort(
  uint8_t* obj_base, std::vector<index_t>::iterator index_start,
  std::vector<index_t>::iterator index_end) const {
  std::sort(index_start, index_end, [obj_base](const auto& a, const auto& b) {
    const uint8_t* aa = obj_base + a;
    const uint8_t* bb = obj_base + b;
    if (*aa != 0xff && *bb != 0xff) {
      const auto m = std::min(*aa - 0x80, *bb - 0x80);
      const int c = std::memcmp(aa + 1, bb + 1, m);
      return c < 0 || (c == 0 && *aa < *bb);
    } else {
      uint32_t lena = 0;
      uint32_t lenb = 0;
      aa = FindAttrName(aa, lena);
      bb = FindAttrName(bb, lenb);
      const auto m = std::min(lena, lenb);
      const int c = std::memcmp(aa, bb, m);
      return c < 0 || (c == 0 && lena < lenb);
    }
  });
}

void Builder::sortObjectIndexLong(
  uint8_t* obj_base, std::vector<index_t>::iterator index_start,
  std::vector<index_t>::iterator index_end) const {
  std::vector<SortEntry> sort_entries;
  const index_t n = std::distance(index_start, index_end);
  SDB_ASSERT(n > 1);
  sort_entries.reserve(n);
  std::for_each(index_start, index_end, [&](auto offset) {
    auto& e = sort_entries.emplace_back();
    e.key = FindAttrName(obj_base + offset, e.len);
    SDB_ASSERT(e.key);
    e.offset = offset;
  });
  absl::c_sort(sort_entries, [](const auto& l, const auto& r) {
    const auto cmp_len = std::min(l.len, r.len);
    int res = std::memcmp(l.key, r.key, cmp_len);
    return res < 0 || (res == 0 && l.len < r.len);
  });
  // copy back the sorted offsets
  for (index_t i = 0; i != n; ++i) {
    index_start[i] = sort_entries[i].offset;
  }
}

Builder& Builder::closeEmptyArrayOrObject(ValueLength pos, bool is_array) {
  // empty Array or Object
  _start[pos] = (is_array ? 0x01 : 0x0a);
  SDB_ASSERT(_pos == pos + 9);
  rollback(8);  // no bytelength and number subvalues needed
  closeLevel();
  return *this;
}

bool Builder::closeCompactArrayOrObject(
  ValueLength pos, bool is_array, std::vector<index_t>::iterator index_start,
  std::vector<index_t>::iterator index_end) {
  const index_t n = std::distance(index_start, index_end);

  // use compact notation
  ValueLength len = GetVariableValueLength(static_cast<ValueLength>(n));
  SDB_ASSERT(len > 0);
  ValueLength byte_size = _pos - (pos + 8) + len;
  SDB_ASSERT(byte_size > 0);
  ValueLength b_len = GetVariableValueLength(byte_size);
  byte_size += b_len;
  if (GetVariableValueLength(byte_size) != b_len) {
    byte_size += 1;
    b_len += 1;
  }

  if (b_len < 9) {
    // can only use compact notation if total byte length is at most 8 bytes
    // long
    _start[pos] = (is_array ? 0x13 : 0x14);
    ValueLength target_pos = 1 + b_len;

    if (_pos > (pos + 9)) {
      ValueLength len = _pos - (pos + 9);
      memmove(_start + pos + target_pos, _start + pos + 9, CheckOverflow(len));
    }

    // store byte length
    SDB_ASSERT(byte_size > 0);
    StoreVariableValueLength<false>(_start + pos + 1, byte_size);

    // need additional memory for storing the number of values
    if (len > 8 - b_len) {
      reserve(len);
    }
    StoreVariableValueLength<true>(_start + pos + byte_size - 1,
                                   static_cast<ValueLength>(n));

    rollback(8);
    advance(len + b_len);

    closeLevel();

#ifdef VPACK_DEBUG
    SDB_ASSERT(_start[pos] == (isArray ? 0x13 : 0x14));
    SDB_ASSERT(byteSize == readVariableValueLength<false>(_start + pos + 1));
    SDB_ASSERT(isArray || _start[pos + 1 + bLen] != 0x00);
    SDB_ASSERT(n == readVariableValueLength<true>(_start + pos + byteSize - 1));
#endif
    return true;
  }
  return false;
}

Builder& Builder::closeArray(ValueLength pos,
                             std::vector<index_t>::iterator index_start,
                             std::vector<index_t>::iterator index_end) {
  const index_t n = std::distance(index_start, index_end);
  SDB_ASSERT(n > 0);

  bool need_index_table = true;
  bool need_nr_subs = true;

  if (n == 1) {
    // just one array entry
    need_index_table = false;
    need_nr_subs = false;
  } else if ((_pos - pos) - index_start[0] ==
             n * (index_start[1] - index_start[0])) {
    // In this case it could be that all entries have the same length
    // and we do not need an offset table at all:
    bool build_index_table = false;
    const ValueLength sub_len = index_start[1] - index_start[0];
    if ((_pos - pos) - index_start[n - 1] != sub_len) {
      build_index_table = true;
    } else {
      for (size_t i = 1; i < n - 1; i++) {
        if (index_start[i + 1] - index_start[i] != sub_len) {
          // different lengths
          build_index_table = true;
          break;
        }
      }
    }

    if (!build_index_table) {
      need_index_table = false;
      need_nr_subs = false;
    }
  }

  SDB_ASSERT(need_index_table == need_nr_subs);

  // First determine byte length and its format:
  unsigned int offset_size;
  // can be 1, 2, 4 or 8 for the byte width of the offsets,
  // the byte length and the number of subvalues:
  bool allow_memmove =
    IsAllowedToMemmove(options, _start + pos, index_start, index_end, 1);
  if (_pos - pos + (need_index_table ? n : 0) -
        (allow_memmove ? (need_nr_subs ? 6 : 7) : 0) <=
      0xff) {
    // We have so far used _pos - pos bytes, including the reserved 8
    // bytes for byte length and number of subvalues. In the 1-byte number
    // case we would win back 6 bytes but would need one byte per subvalue
    // for the index table
    offset_size = 1;
  } else {
    allow_memmove =
      IsAllowedToMemmove(options, _start + pos, index_start, index_end, 2);
    if (_pos - pos + (need_index_table ? 2 * n : 0) -
          (allow_memmove ? (need_nr_subs ? 4 : 6) : 0) <=
        0xffff) {
      offset_size = 2;
    } else {
      allow_memmove = false;
      if (_pos - pos + (need_index_table ? 4 * n : 0) <= 0xffffffffu) {
        offset_size = 4;
      } else {
        offset_size = 8;
      }
    }
  }

  SDB_ASSERT(offset_size == 1 || offset_size == 2 || offset_size == 4 ||
             offset_size == 8);
  SDB_ASSERT(!allow_memmove || offset_size == 1 || offset_size == 2);

  if (offset_size < 8 && !need_index_table &&
      options->padding_behavior == Options::PaddingBehavior::kUsePadding) {
    // if we are allowed to use padding, we will pad to 8 bytes anyway. as we
    // are not using an index table, we can also use type 0x05 for all Arrays
    // without making things worse space-wise
    offset_size = 8;
    allow_memmove = false;
  }

  // fix head byte
  _start[pos] = DetermineArrayType(need_index_table, offset_size);

  // Maybe we need to move down data:
  if (allow_memmove) {
    // check if one of the first entries in the array is ValueType::None
    // (0x00). in this case, we could not distinguish between a None (0x00)
    // and the optional padding. so we must prevent the memmove here
    ValueLength target_pos = 1 + 2 * offset_size;
    if (!need_index_table) {
      target_pos -= offset_size;
    }
    if (_pos > (pos + 9)) {
      ValueLength len = _pos - (pos + 9);
      memmove(_start + pos + target_pos, _start + pos + 9, CheckOverflow(len));
    }
    const ValueLength diff = 9 - target_pos;
    rollback(diff);
    if (need_index_table) {
      for (size_t i = 0; i < n; i++) {
        index_start[i] -= diff;
      }
    }  // Note: if !needIndexTable the index array is now wrong!
  }

  // Now build the table:
  if (need_index_table) {
    reserve(offset_size * n + (offset_size == 8 ? 8 : 0));
    ValueLength table_base = _pos;
    advance(offset_size * n);
    for (size_t i = 0; i < n; i++) {
      uint64_t x = index_start[i];
      for (size_t j = 0; j < offset_size; j++) {
        _start[table_base + offset_size * i + j] = x & 0xff;
        x >>= 8;
      }
    }
  }

  // Finally fix the byte width at tthe end:
  if (offset_size == 8 && need_nr_subs) {
    reserve(8);
    storeUnsigned(_start + _pos, 8, n);
  }

  // Fix the byte length in the beginning:
  ValueLength x = _pos - pos;
  for (unsigned int i = 1; i <= offset_size; i++) {
    _start[pos + i] = x & 0xff;
    x >>= 8;
  }

  if (offset_size < 8 && need_nr_subs) {
    x = n;
    for (unsigned int i = offset_size + 1; i <= 2 * offset_size; i++) {
      _start[pos + i] = x & 0xff;
      x >>= 8;
    }
  }

  // Now the array or object is complete, we pop a ValueLength
  // off the _stack:
  closeLevel();
  return *this;
}

Builder& Builder::close() {
  if (isClosed()) [[unlikely]] {
    throw Exception(Exception::kBuilderNeedOpenCompound);
  }
  SDB_ASSERT(!_stack.empty());
  const ValueLength pos = _stack.back().start_pos;
  const ValueLength index_start_pos = _stack.back().index_start_pos;
  const uint8_t head = _start[pos];

  SDB_ASSERT(head == 0x06 || head == 0x0b || head == 0x13 || head == 0x14);

  const bool is_array = (head == 0x06 || head == 0x13);
  auto index_start = _indexes.begin() + index_start_pos;
  auto index_end = _indexes.end();
  const ValueLength n = std::distance(index_start, index_end);

  if (n == 0) {
    closeEmptyArrayOrObject(pos, is_array);
    return *this;
  }

  // From now on index.size() > 0
  SDB_ASSERT(n > 0);

  // check if we can use the compact Array / Object format
  if (head == 0x13 || head == 0x14 ||
      (head == 0x06 && options->build_unindexed_arrays) ||
      (head == 0x0b && (options->build_unindexed_objects || n == 1))) {
    if (closeCompactArrayOrObject(pos, is_array, index_start, index_end)) {
      // And, if desired, check attribute uniqueness:
      if ((head == 0x0b || head == 0x14) &&
          options->check_attribute_uniqueness && n > 1 &&
          !checkAttributeUniqueness(Slice(_start + pos))) {
        // duplicate attribute name!
        throw Exception(Exception::kDuplicateAttributeName);
      }
      return *this;
    }
    // This might fall through, if closeCompactArrayOrObject gave up!
  }

  if (is_array) {
    closeArray(pos, _indexes.begin() + index_start_pos, _indexes.end());
    return *this;
  }

  // from here on we are sure that we are dealing with Object types only.

  // fix head byte in case a compact Array / Object was originally requested
  _start[pos] = 0x0b;

  // First determine byte length and its format:
  unsigned int offset_size = 8;
  // can be 1, 2, 4 or 8 for the byte width of the offsets,
  // the byte length and the number of subvalues:
  // 8 bytes - object length (1 byte) - number of items (1 byte) = 6 bytes
  if (_pos - pos + n - 6 +
        (options->padding_behavior == Options::PaddingBehavior::kUsePadding
           ? 6
           : 0) <=
      0xff) {
    // We have so far used _pos - pos bytes, including the reserved 8
    // bytes for byte length and number of subvalues. In the 1-byte number
    // case we would win back 6 bytes but would need one byte per subvalue
    // for the index table
    offset_size = 1;
    // One could move down things in the offsetSize == 2 case as well,
    // since we only need 4 bytes in the beginning. However, saving these
    // 4 bytes has been sacrificed on the Altar of Performance.
    // 8 bytes - object length (2 bytes) - number of items (2 bytes) = 4 bytes
  } else if (_pos - pos + 2 * n +
               (options->padding_behavior ==
                    Options::PaddingBehavior::kUsePadding
                  ? 4
                  : 0) <=
             0xffff) {
    offset_size = 2;
  } else if (_pos - pos + 4 * n <= 0xffffffffu) {
    offset_size = 4;
  }

  if (offset_size < 4 &&
      (options->padding_behavior == Options::PaddingBehavior::kNoPadding ||
       (offset_size == 1 &&
        options->padding_behavior == Options::PaddingBehavior::kFlexible))) {
    // Maybe we need to move down data:
    ValueLength target_pos = 1 + 2 * offset_size;
    if (_pos > (pos + 9)) {
      ValueLength len = _pos - (pos + 9);
      memmove(_start + pos + target_pos, _start + pos + 9, CheckOverflow(len));
    }
    const ValueLength diff = 9 - target_pos;
    rollback(diff);
    for (size_t i = 0; i < n; i++) {
      index_start[i] -= diff;
    }
  }

  // Now build the table:
  reserve(offset_size * n + (offset_size == 8 ? 8 : 0));
  ValueLength table_base = _pos;
  advance(offset_size * n);
  // Object
  if (n >= 2) {
    sortObjectIndex(_start + pos, index_start, index_end);
  }
  for (size_t i = 0; i < n; ++i) {
    uint64_t x = index_start[i];
    for (size_t j = 0; j < offset_size; ++j) {
      _start[table_base + offset_size * i + j] = x & 0xff;
      x >>= 8;
    }
  }
  // Finally fix the byte width in the type byte:
  if (offset_size > 1) {
    if (offset_size == 2) {
      _start[pos] += 1;
    } else if (offset_size == 4) {
      _start[pos] += 2;
    } else {  // offsetSize == 8
      _start[pos] += 3;
      // write number of items
      reserve(8);
      storeUnsigned(_start + _pos, 8, n);
    }
  }

  // Fix the byte length in the beginning:
  const ValueLength byte_length = _pos - pos;
  ValueLength x = byte_length;
  for (unsigned int i = 1; i <= offset_size; i++) {
    _start[pos + i] = x & 0xff;
    x >>= 8;
  }

  if (offset_size < 8) {
    ValueLength x = n;
    for (unsigned int i = offset_size + 1; i <= 2 * offset_size; i++) {
      _start[pos + i] = x & 0xff;
      x >>= 8;
    }
  }

#ifdef VPACK_DEBUG
  // make sure byte size and number of items are actually written correctly
  if (offsetSize == 1) {
    SDB_ASSERT(_start[pos] == 0x0b);
    // read byteLength
    [[maybe_unused]] uint64_t v =
      readIntegerFixed<uint64_t, 1>(_start + pos + 1);
    SDB_ASSERT(byteLength == v);
    // read byteLength n
    v = readIntegerFixed<uint64_t, 1>(_start + pos + 1 + offsetSize);
    SDB_ASSERT(n == v);
  } else if (offsetSize == 2) {
    SDB_ASSERT(_start[pos] == 0x0c);
    // read byteLength
    [[maybe_unused]] uint64_t v =
      readIntegerFixed<uint64_t, 2>(_start + pos + 1);
    SDB_ASSERT(byteLength == v);
    // read byteLength n
    v = readIntegerFixed<uint64_t, 2>(_start + pos + 1 + offsetSize);
    SDB_ASSERT(n == v);
  } else if (offsetSize == 4) {
    SDB_ASSERT(_start[pos] == 0x0d);
    // read byteLength
    [[maybe_unused]] uint64_t v =
      readIntegerFixed<uint64_t, 4>(_start + pos + 1);
    SDB_ASSERT(byteLength == v);
    // read byteLength n
    v = readIntegerFixed<uint64_t, 4>(_start + pos + 1 + offsetSize);
    SDB_ASSERT(n == v);
  } else if (offsetSize == 8) {
    SDB_ASSERT(_start[pos] == 0x0e);
    [[maybe_unused]] uint64_t v =
      readIntegerFixed<uint64_t, 4>(_start + pos + 1);
    SDB_ASSERT(byteLength == v);
  }
#endif

  // And, if desired, check attribute uniqueness:
  if (options->check_attribute_uniqueness && n > 1 &&
      !checkAttributeUniqueness(Slice(_start + pos))) {
    // duplicate attribute name!
    throw Exception(Exception::kDuplicateAttributeName);
  }

  // Now the array or object is complete, we pop a ValueLength
  // off the _stack:
  closeLevel();

  return *this;
}

// return the value for a specific key of an Object value
Slice Builder::getKey(std::string_view key) const {
  if (_stack.empty()) [[unlikely]] {
    throw Exception(Exception::kBuilderNeedOpenObject);
  }
  SDB_ASSERT(!_stack.empty());
  const ValueLength pos = _stack.back().start_pos;
  const ValueLength index_start_pos = _stack.back().index_start_pos;
  if (_start[pos] != 0x0b && _start[pos] != 0x14) [[unlikely]] {
    throw Exception(Exception::kBuilderNeedOpenObject);
  }
  auto index_start = _indexes.begin() + index_start_pos;
  auto index_end = _indexes.end();
  while (index_start != index_end) {
    Slice s(_start + pos + *index_start);
    if (s.isEqualString(key)) {
      return Slice(s.start() + s.byteSize());
    }
    ++index_start;
  }
  return Slice();
}

uint8_t* Builder::set(Value item) {
  checkKeyHasValidType(false);
  const auto old_pos = _pos;
  // This method builds a single further VPack item at the current
  // append position. If this is an array or object, then an index
  // table is created and a new ValueLength is pushed onto the stack.
  switch (item.valueType()) {
    case ValueType::Null: {
      appendByte(0x18);
    } break;
    case ValueType::Array: {
      addArray(item.unindexed());
    } break;
    case ValueType::Object: {
      addObject(item.unindexed());
    } break;
    default:
      throw Exception{Exception::kBuilderUnexpectedType};
  }
  return _start + old_pos;
}

uint8_t* Builder::set(Slice item) {
  checkKeyHasValidType(item);
  const ValueLength l = item.byteSize();
  if (l != 0) {
    reserve(l);
    SDB_ASSERT(item.start() != nullptr);
    std::memcpy(_start + _pos, item.start(), CheckOverflow(l));
    advance(l);
  }
  return _start + _pos - l;
}

uint8_t* Builder::set(std::string_view item) {
  checkKeyHasValidType(true);
  const auto old_pos = _pos;
  auto size = item.size();
  if (size <= 126) {
    // short string
    reserve(1 + size);
    appendByteUnchecked(static_cast<uint8_t>(0x80 + size));
  } else {
    // long string
    if (size > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
      throw Exception{Exception::kInternalError,
                      "vpack::Builder is unable to allocate string"};
    }
    reserve(1 + 4 + size);
    storeUnsigned(_start + _pos, 0xff, 4, static_cast<uint32_t>(size));
  }
  if (size != 0) {
    SDB_ASSERT(item.data() != nullptr);
    std::memcpy(_start + _pos, item.data(), size);
    advance(size);
  }
  return _start + old_pos;
}

uint8_t* Builder::set(const IStringFromParts& parts) {
  checkKeyHasValidType(true);
  // This method builds a single VPack String item composed of the n parts.
  const auto old_pos = _pos;
  auto length = parts.totalLength();
  if (length <= 126) {
    // short string
    reserve(1 + length);
    appendByteUnchecked(static_cast<uint8_t>(0x80 + length));
  } else {
    // long string
    if (length > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
      throw Exception{Exception::kInternalError,
                      "vpack::Builder is unable to allocate string from parts"};
    }
    reserve(1 + 4 + length);
    storeUnsigned(_start + _pos, 0xff, 4, static_cast<uint32_t>(length));
  }
  for (size_t idx = 0, size = parts.numberOfParts(); idx != size; ++idx) {
    auto part = parts(idx);
    if (part.size() != 0) {
      std::memcpy(_start + _pos, part.data(), part.size());
      advance(part.size());
    }
  }
  return _start + old_pos;
}

void Builder::cleanupAdd() noexcept {
  SDB_ASSERT(!_stack.empty());
  SDB_ASSERT(!_indexes.empty());
  _indexes.pop_back();
}

void Builder::reportAdd() {
  SDB_ASSERT(!_stack.empty());
  if (_indexes.capacity() == 0) {
    // make an initial reservation for several items at
    // a time, in order to save frequent reallocations for
    // the first few attributes
    _indexes.reserve(8);
  }
  _indexes.push_back(_pos - _stack.back().start_pos);
}

void Builder::closeLevel() noexcept {
  SDB_ASSERT(!_stack.empty());
  const auto index_start_pos = _stack.back().index_start_pos;
  _stack.pop_back();
  _indexes.erase(_indexes.begin() + index_start_pos, _indexes.end());
}

bool Builder::checkAttributeUniqueness(Slice obj) const {
  SDB_ASSERT(options->check_attribute_uniqueness == true);
  SDB_ASSERT(obj.isObject());
  SDB_ASSERT(obj.length() >= 2);

  if (obj.isSorted()) {
    // object attributes are sorted
    return checkAttributeUniquenessSorted(obj);
  }

  return checkAttributeUniquenessUnsorted(obj);
}

bool Builder::checkAttributeUniquenessSorted(Slice obj) const {
  ObjectIterator it{obj, false};
  SDB_ASSERT(it.size() > 1);
  auto key = (*it).key;
  SDB_ASSERT(key.isString());
  auto prev = key.stringViewUnchecked();
  it.next();
  do {
    key = (*it).key;
    SDB_ASSERT(key.isString());
    auto curr = key.stringViewUnchecked();
    if (prev == curr) {
      return false;
    }
    prev = curr;
    it.next();
  } while (it.valid());
  return true;
}

bool Builder::checkAttributeUniquenessUnsorted(Slice obj) const {
  // cutoff value for linear attribute uniqueness scan
  // unsorted objects with this amount of attributes (or less) will
  // be validated using a non-allocating scan over the attributes
  // objects with more attributes will use a validation routine that
  // will use an absl::flat_hash_set for O(1) lookups but with heap
  // allocations
  ObjectIterator it{obj, true};

  if (it.size() <= kLinearAttributeUniquenessCutoff) {
    return CheckAttributeUniquenessUnsortedBrute(it);
  }
  return CheckAttributeUniquenessUnsortedSet(it);
}

uint8_t* Builder::add(ObjectIterator&& it) {
  if (_stack.empty()) [[unlikely]] {
    throw Exception{Exception::kBuilderNeedOpenObject};
  }
  const auto pos = _stack.back().start_pos;
  if (_start[pos] != 0x0b && _start[pos] != 0x14) [[unlikely]] {
    throw Exception(Exception::kBuilderNeedOpenObject);
  }
  if (_key_written) [[unlikely]] {
    throw Exception{Exception::kBuilderKeyAlreadyWritten};
  }
  const auto old_pos = _pos;
  for (auto [k, v] : it) {
    add(k);
    add(v);
  }
  return _start + old_pos;
}

uint8_t* Builder::add(ArrayIterator&& it) {
  if (_stack.empty()) [[unlikely]] {
    throw Exception{Exception::kBuilderNeedOpenArray};
  }
  const auto pos = _stack.back().start_pos;
  if (_start[pos] != 0x06 && _start[pos] != 0x13) [[unlikely]] {
    throw Exception{Exception::kBuilderNeedOpenArray};
  }
  const auto old_pos = _pos;
  for (auto v : it) {
    add(v);
  }
  return _start + old_pos;
}

// because 128 % 16 == 0 it's high probability that it's allocator class size
// worse to loose 8 byte here than for the next allocation size (see shared_ptr)
static_assert(sizeof(Builder) == 128);

// TODO(mbkkt) I think it should be reworked to make
//  sizeof(BufferUInt8) == 16(15/8 sso) or 24(23/16 sso) bytes
// Never allocated without make_shared, so it doesn't really matter
static_assert(sizeof(BufferUInt8) == 232);

#ifdef _LIBCPP_VERSION
// because 256 % 256 == 0 it's high probability that it's allocator class size
static_assert(
  sizeof(std::__shared_ptr_emplace<BufferUInt8, std::allocator<BufferUInt8>>) ==
  256);

static_assert(
  sizeof(std::__shared_ptr_emplace<Builder, std::allocator<Builder>>) ==
  128 + 24);
#endif

static_assert(sizeof(double) == 8);

}  // namespace vpack
