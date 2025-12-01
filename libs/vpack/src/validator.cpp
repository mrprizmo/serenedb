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

#include "vpack/validator.h"

#include <absl/container/flat_hash_set.h>

#include <memory>

#include "asm-functions.h"
#include "vpack/common.h"
#include "vpack/exception.h"
#include "vpack/slice.h"
#include "vpack/value_type.h"

using namespace vpack;

template<bool Reverse>
static ValueLength ReadVariableLengthValue(const uint8_t*& p,
                                           const uint8_t* end) {
  ValueLength value = 0;
  ValueLength shifter = 0;
  while (true) {
    uint8_t c = *p;
    SDB_ASSERT(shifter <= 8 * 7);
    value += static_cast<ValueLength>(c & 0x7fU) << shifter;
    shifter += 7;
    if (Reverse) {
      --p;
    } else {
      ++p;
    }
    if (!(c & 0x80U)) {
      break;
    }
    if (p == end || shifter > 7 * 8) [[unlikely]] {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Compound value length value is out of bounds");
    }
  }
  return value;
}

Validator::Validator(const Options* options) : options(options), _nesting(0) {
  if (options == nullptr) {
    throw Exception(Exception::kInternalError, "Options cannot be a nullptr");
  }
}

bool Validator::validate(const uint8_t* ptr, size_t length, bool is_sub_part) {
  // reset internal state
  _nesting = 0;
  validatePart(ptr, length, is_sub_part);
  return true;
}

void Validator::validatePart(const uint8_t* ptr, size_t length,
                             bool is_sub_part) {
  if (length == 0) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "length 0 is invalid for any VPack value");
  }

  uint8_t head = *ptr;

  // type() only reads the first byte, which is safe
  const ValueType type = Slice(ptr).type();

  if (type == ValueType::None && head != 0x00U) {
    // invalid type
    throw Exception(Exception::kValidatorInvalidType);
  }

  // special handling for certain types...
  switch (type) {
    case ValueType::None:
    case ValueType::Null:
    case ValueType::Bool:
    case ValueType::SmallInt:
    case ValueType::Int:
    case ValueType::UInt:
    case ValueType::Double:
      break;
    case ValueType::String: {
      const uint8_t* p = nullptr;
      uint32_t len = 0;
      if (head == 0xff) {
        validateBufferLength(1 + 4, length, true);
        len = ReadIntegerFixed<uint32_t, 4>(ptr + 1);
        p = ptr + 1 + 4;
        validateBufferLength(len + 1 + 4, length, true);
      } else {
        len = head - 0x80;
        p = ptr + 1;
        validateBufferLength(len + 1, length, true);
      }

      if (options->validate_utf8_strings &&
          !gValidateUtf8String(p, static_cast<size_t>(len))) {
        throw Exception(Exception::kInvalidUtf8Sequence);
      }
    } break;
    case ValueType::Array: {
      if (++_nesting >= options->nesting_limit) {
        throw Exception(Exception::kTooDeepNesting);
      }
      validateArray(ptr, length);
      --_nesting;
    } break;
    case ValueType::Object: {
      if (++_nesting >= options->nesting_limit) {
        throw Exception(Exception::kTooDeepNesting);
      }
      validateObject(ptr, length);
      --_nesting;
    } break;
  }

  // common validation that must happen for all types
  validateSliceLength(ptr, length, is_sub_part);
}

void Validator::validateArray(const uint8_t* ptr, size_t length) {
  uint8_t head = *ptr;

  if (head == 0x13U) {
    // compact array
    validateCompactArray(ptr, length);
  } else if (head >= 0x02U && head <= 0x05U) {
    // array without index table
    validateUnindexedArray(ptr, length);
  } else if (head >= 0x06U && head <= 0x09U) {
    // array with index table
    validateIndexedArray(ptr, length);
  } else if (head == 0x01U) {
    // empty array. always valid
  }
}

void Validator::validateCompactArray(const uint8_t* ptr, size_t length) {
  // compact Array without index table
  validateBufferLength(4, length, true);

  const uint8_t* p = ptr + 1;
  // read byteLength
  const ValueLength byte_size = ReadVariableLengthValue<false>(p, p + length);
  if (byte_size > length || byte_size < 4) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array length value is out of bounds");
  }

  // read nrItems
  const uint8_t* data = p;
  p = ptr + byte_size - 1;
  ValueLength nr_items = ReadVariableLengthValue<true>(p, ptr + byte_size);
  if (nr_items == 0) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array length value is out of bounds");
  }
  ++p;

  // validate the array members
  const uint8_t* e = p;
  p = data;
  while (nr_items-- > 0) {
    if (p >= e) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array items number is out of bounds");
    }
    validatePart(p, e - p, true);
    p += Slice(p).byteSize();
  }
}

void Validator::validateUnindexedArray(const uint8_t* ptr, size_t length) {
  // Array without index table, with 1-8 bytes lengths, all values with same
  // length
  uint8_t head = *ptr;
  const ValueLength byte_size_length =
    1ULL << (static_cast<ValueLength>(head) - 0x02U);
  validateBufferLength(1 + byte_size_length + 1, length, true);
  const ValueLength byte_size =
    ReadIntegerNonEmpty<ValueLength>(ptr + 1, byte_size_length);

  if (byte_size > length) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array length is out of bounds");
  }

  // look up first member
  const uint8_t* p = ptr + 1 + byte_size_length;
  const uint8_t* e = p + (8 - byte_size_length);

  if (e > ptr + byte_size) {
    e = ptr + byte_size;
  }
  while (p < e && *p == '\x00') {
    ++p;
  }

  if (p >= ptr + byte_size) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array structure is invalid");
  }

  // check if padding is correct
  if (p != ptr + 1 + byte_size_length &&
      p != ptr + 1 + byte_size_length + (8 - byte_size_length)) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array padding is invalid");
  }

  validatePart(p, length - (p - ptr), true);
  ValueLength item_size = Slice(p).byteSize();
  if (item_size == 0) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array itemSize value is invalid");
  }
  ValueLength nr_items = (byte_size - (p - ptr)) / item_size;

  if (nr_items == 0) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array nrItems value is invalid");
  }
  // we already validated p, so move it forward
  p += item_size;
  e = ptr + length;
  --nr_items;

  while (nr_items > 0) {
    if (p >= e) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array value is out of bounds");
    }
    // validate sub value
    validatePart(p, e - p, true);
    if (Slice(p).byteSize() != item_size) {
      // got a sub-object with a different size. this is not allowed
      throw Exception(Exception::kValidatorInvalidLength,
                      "Unexpected Array value length");
    }
    p += item_size;
    --nr_items;
  }
}

void Validator::validateIndexedArray(const uint8_t* ptr, size_t length) {
  // Array with index table, with 1-8 bytes lengths
  uint8_t head = *ptr;
  const ValueLength byte_size_length =
    1ULL << (static_cast<ValueLength>(head) - 0x06U);
  validateBufferLength(1 + byte_size_length + byte_size_length + 1, length,
                       true);
  ValueLength byte_size =
    ReadIntegerNonEmpty<ValueLength>(ptr + 1, byte_size_length);

  if (byte_size > length) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array length is out of bounds");
  }

  ValueLength nr_items;
  const uint8_t* index_table;
  const uint8_t* first_member;

  if (head == 0x09U) {
    // byte length = 8
    nr_items = ReadIntegerNonEmpty<ValueLength>(
      ptr + byte_size - byte_size_length, byte_size_length);

    if (nr_items == 0) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array nrItems value is invalid");
    }

    index_table =
      ptr + byte_size - byte_size_length - (nr_items * byte_size_length);
    if (index_table < ptr + byte_size_length ||
        index_table > ptr + length - byte_size_length - byte_size_length) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array index table is out of bounds");
    }

    first_member = ptr + 1 + byte_size_length;
  } else {
    // byte length = 1, 2 or 4
    nr_items = ReadIntegerNonEmpty<ValueLength>(ptr + 1 + byte_size_length,
                                                byte_size_length);

    if (nr_items == 0) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array nrItems value is invalid");
    }

    // look up first member
    const uint8_t* p = ptr + 1 + byte_size_length + byte_size_length;
    const uint8_t* e = p + (8 - byte_size_length - byte_size_length);
    if (e > ptr + byte_size) {
      e = ptr + byte_size;
    }
    while (p < e && *p == '\x00') {
      ++p;
    }

    // check if padding is correct
    if (p != ptr + 1 + byte_size_length + byte_size_length &&
        p != ptr + 1 + byte_size_length + byte_size_length +
               (8 - byte_size_length - byte_size_length)) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array padding is invalid");
    }

    index_table = ptr + byte_size - (nr_items * byte_size_length);
    if (index_table < ptr + byte_size_length + byte_size_length ||
        index_table < p || index_table > ptr + length - byte_size_length) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array index table is out of bounds");
    }

    first_member = p;
  }

  SDB_ASSERT(nr_items > 0);

  ValueLength actual_nr_items = 0;
  const uint8_t* member = first_member;
  while (member < index_table) {
    validatePart(member, index_table - member, true);
    ValueLength offset = ReadIntegerNonEmpty<ValueLength>(
      index_table + actual_nr_items * byte_size_length, byte_size_length);
    if (offset != static_cast<ValueLength>(member - ptr)) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Array index table is wrong");
    }

    member += Slice(member).byteSize();
    ++actual_nr_items;
  }

  if (actual_nr_items != nr_items) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Array has more items than in index");
  }
}

void Validator::validateObject(const uint8_t* ptr, size_t length) {
  uint8_t head = *ptr;

  if (head == 0x14U) {
    // compact object
    validateCompactObject(ptr, length);
  } else if (head >= 0x0bU && head <= 0x12U) {
    // regular object
    validateIndexedObject(ptr, length);
  } else if (head == 0x0aU) {
    // empty object. always valid
  }
}

void Validator::validateCompactObject(const uint8_t* ptr, size_t length) {
  // compact Object without index table
  validateBufferLength(5, length, true);

  const uint8_t* p = ptr + 1;
  // read byteLength
  const ValueLength byte_size = ReadVariableLengthValue<false>(p, p + length);
  if (byte_size > length || byte_size < 5) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Object length value is out of bounds");
  }

  // read nrItems
  const uint8_t* data = p;
  p = ptr + byte_size - 1;
  ValueLength nr_items = ReadVariableLengthValue<true>(p, ptr + byte_size);
  if (nr_items == 0) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Object length value is out of bounds");
  }
  ++p;

  // validate the object members
  const uint8_t* e = p;
  p = data;
  while (nr_items-- > 0) {
    if (p >= e) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object items number is out of bounds");
    }
    // validate key
    validatePart(p, e - p, true);
    Slice key(p);
    if (!key.isString()) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Invalid object key type");
    }
    ValueLength key_size = key.byteSize();
    // validate key
    if (options->validate_utf8_strings) {
      validatePart(p, key_size, true);
    }

    // validate value
    p += key_size;
    validatePart(p, e - p, true);
    p += Slice(p).byteSize();
  }

  // finally check if we are now pointing at the end or not
  if (p != e) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Object has more members than specified");
  }
}

void Validator::validateIndexedObject(const uint8_t* ptr, size_t length) {
  // Object with index table, with 1-8 bytes lengths
  uint8_t head = *ptr;
  ValueLength byte_size_length;
  if (head >= 0x0f) [[unlikely]] {
    byte_size_length = 1ULL << (static_cast<ValueLength>(head) - 0x0fU);
  } else {
    byte_size_length = 1ULL << (static_cast<ValueLength>(head) - 0x0bU);
  }
  SDB_ASSERT(byte_size_length > 0);
  validateBufferLength(1 + byte_size_length + byte_size_length + 1, length,
                       true);
  const ValueLength byte_size =
    ReadIntegerNonEmpty<ValueLength>(ptr + 1, byte_size_length);

  if (byte_size > length) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Object length is out of bounds");
  }

  ValueLength nr_items;
  const uint8_t* index_table;
  const uint8_t* first_member;

  if (head == 0x0eU || head == 0x12U) {
    // byte length = 8
    nr_items = ReadIntegerNonEmpty<ValueLength>(
      ptr + byte_size - byte_size_length, byte_size_length);

    if (nr_items == 0) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object nrItems value is invalid");
    }

    index_table =
      ptr + byte_size - byte_size_length - (nr_items * byte_size_length);
    if (index_table < ptr + byte_size_length ||
        index_table > ptr + length - byte_size_length - byte_size_length) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object index table is out of bounds");
    }

    first_member = ptr + byte_size;
  } else {
    // byte length = 1, 2 or 4
    nr_items = ReadIntegerNonEmpty<ValueLength>(ptr + 1 + byte_size_length,
                                                byte_size_length);

    if (nr_items == 0) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object nrItems value is invalid");
    }

    // look up first member
    const uint8_t* p = ptr + 1 + byte_size_length + byte_size_length;
    const uint8_t* e = p + (8 - byte_size_length - byte_size_length);
    if (e > ptr + byte_size) {
      e = ptr + byte_size;
    }
    while (p < e && *p == '\x00') {
      ++p;
    }

    // check if padding is correct
    if (p != ptr + 1 + byte_size_length + byte_size_length &&
        p != ptr + 1 + byte_size_length + byte_size_length +
               (8 - byte_size_length - byte_size_length)) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object padding is invalid");
    }

    index_table = ptr + byte_size - (nr_items * byte_size_length);
    if (index_table < ptr + byte_size_length + byte_size_length ||
        index_table < p || index_table > ptr + length - byte_size_length) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object index table is out of bounds");
    }

    first_member = p;
  }

  SDB_ASSERT(nr_items > 0);

  ValueLength table_buf[16];  // Fixed space to save offsets found sequentially
  ValueLength* table = table_buf;
  std::unique_ptr<ValueLength[]> table_guard;
  std::unique_ptr<absl::flat_hash_set<ValueLength>> offset_set;
  if (nr_items > 16) {
    if (nr_items <= 128) {
      table = new ValueLength[nr_items];  // throws if bad_alloc
      table_guard.reset(table);           // for automatic deletion
    } else {
      // if we have even more items, we directly create an flat_hash_set
      offset_set = std::make_unique<absl::flat_hash_set<ValueLength>>();
    }
  }
  ValueLength actual_nr_items = 0;
  const uint8_t* member = first_member;
  while (member < index_table) {
    validatePart(member, index_table - member, true);

    Slice key(member);
    if (!key.isString()) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Invalid object key type");
    }

    const ValueLength key_size = key.byteSize();
    if (options->validate_utf8_strings) {
      validatePart(member, key_size, true);
    }

    const uint8_t* value = member + key_size;
    if (value >= index_table) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object value leaking into index table");
    }
    validatePart(value, index_table - value, true);

    ValueLength offset = static_cast<ValueLength>(member - ptr);
    if (nr_items <= 128) {
      table[actual_nr_items] = offset;
    } else {
      offset_set->emplace(offset);
    }

    member += key_size + Slice(value).byteSize();
    ++actual_nr_items;

    if (actual_nr_items > nr_items) {
      throw Exception(Exception::kValidatorInvalidLength,
                      "Object value has more key/value pairs than announced");
    }
  }

  if (actual_nr_items < nr_items) {
    throw Exception(Exception::kValidatorInvalidLength,
                    "Object has fewer items than in index");
  }

  // Finally verify each offset in the index:
  if (nr_items <= 128) {
    for (ValueLength pos = 0; pos < nr_items; ++pos) {
      ValueLength offset = ReadIntegerNonEmpty<ValueLength>(
        index_table + pos * byte_size_length, byte_size_length);
      // Binary search in sorted index list:
      ValueLength low = 0;
      ValueLength high = nr_items;
      bool found = false;
      while (low < high) {
        ValueLength mid = (low + high) / 2;
        if (offset == table[mid]) {
          found = true;
          break;
        } else if (offset < table[mid]) {
          high = mid;
        } else {  // offset > table[mid]
          low = mid + 1;
        }
      }
      if (!found) {
        throw Exception(Exception::kValidatorInvalidLength,
                        "Object has invalid index offset");
      }
    }
  } else {
    for (ValueLength pos = 0; pos < nr_items; ++pos) {
      ValueLength offset = ReadIntegerNonEmpty<ValueLength>(
        index_table + pos * byte_size_length, byte_size_length);
      auto i = offset_set->find(offset);
      if (i == offset_set->end()) {
        throw Exception(Exception::kValidatorInvalidLength,
                        "Object has invalid index offset");
      }
      offset_set->erase(i);
    }
  }
}

void Validator::validateBufferLength(size_t expected, size_t actual,
                                     bool is_sub_part) {
  if ((expected > actual) || (expected != actual && !is_sub_part)) {
    throw Exception(
      Exception::kValidatorInvalidLength,
      "given buffer length is unequal to actual length of Slice in buffer");
  }
}

void Validator::validateSliceLength(const uint8_t* ptr, size_t length,
                                    bool is_sub_part) {
  size_t actual = static_cast<size_t>(Slice(ptr).byteSize());
  validateBufferLength(actual, length, is_sub_part);
}
