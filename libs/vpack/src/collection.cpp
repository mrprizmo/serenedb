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

#include "vpack/collection.h"

#include <absl/algorithm/container.h>
#include <absl/container/flat_hash_map.h>

#include <string>
#include <string_view>

#include "vpack/common.h"
#include "vpack/iterator.h"
#include "vpack/slice.h"
#include "vpack/value.h"
#include "vpack/value_type.h"

using namespace vpack;

// fully append an array to the builder
Builder& Collection::appendArray(Builder& builder, Slice slice) {
  ArrayIterator it(slice);

  while (it.valid()) {
    builder.add(it.value());
    it.next();
  }

  return builder;
}

static absl::flat_hash_set<std::string> MakeSet(
  const std::vector<std::string>& keys) {
  return {keys.begin(), keys.end()};
}

void Collection::forEach(Slice slice, const Predicate& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    if (!predicate(it.value(), index)) {
      // abort
      return;
    }
    it.next();
    ++index;
  }
}

Builder Collection::filter(Slice slice, const Predicate& predicate) {
  // construct a new Array
  Builder b;
  b.add(Value(ValueType::Array));

  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      b.add(s);
    }
    it.next();
    ++index;
  }
  b.close();
  return b;
}

Slice Collection::find(Slice slice, const Predicate& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      return s;
    }
    it.next();
    ++index;
  }

  return Slice();
}

bool Collection::contains(Slice slice, const Predicate& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      return true;
    }
    it.next();
    ++index;
  }

  return false;
}

bool Collection::contains(Slice slice, Slice other) {
  ArrayIterator it(slice);

  while (it.valid()) {
    if (it.value().binaryEquals(other)) {
      return true;
    }
    it.next();
  }

  return false;
}

ValueLength Collection::indexOf(Slice slice, Slice other) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    if (it.value().binaryEquals(other)) {
      return index;
    }
    it.next();
    ++index;
  }

  return Collection::kNotFound;
}

bool Collection::all(Slice slice, const Predicate& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (!predicate(s, index)) {
      return false;
    }
    it.next();
    ++index;
  }

  return true;
}

bool Collection::any(Slice slice, const Predicate& predicate) {
  ArrayIterator it(slice);
  ValueLength index = 0;

  while (it.valid()) {
    Slice s = it.value();
    if (predicate(s, index)) {
      return true;
    }
    it.next();
    ++index;
  }

  return false;
}

std::vector<std::string> Collection::keys(Slice slice) {
  std::vector<std::string> result;

  keys(slice, result);

  return result;
}

Builder Collection::concat(Slice slice1, Slice slice2) {
  Builder b;
  b.openArray();
  appendArray(b, slice1);
  appendArray(b, slice2);
  b.close();

  return b;
}

Builder Collection::extract(Slice slice, int64_t from, int64_t to) {
  Builder b;
  b.openArray();

  int64_t length = static_cast<int64_t>(slice.length());
  int64_t skip = from;
  int64_t limit = to;

  if (limit < 0) {
    limit = length + limit - skip;
  }
  if (limit > 0) {
    ArrayIterator it(slice);
    while (it.valid()) {
      if (skip > 0) {
        --skip;
      } else {
        b.add(it.value());
        if (--limit == 0) {
          break;
        }
      }
      it.next();
    }
  }
  b.close();

  return b;
}

Builder Collection::values(Slice slice) {
  Builder b;
  b.add(Value(ValueType::Array));

  ObjectIterator it(slice, /*useSequentialIteration*/ false);

  while (it.valid()) {
    b.add((*it).value());
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::keep(Slice slice, const std::vector<std::string>& keys) {
  // check if there are so many keys that we want to use the hash-based version
  // cut-off values are arbitrary...
  if (keys.size() >= 4 && slice.length() > 10) {
    return keep(slice, MakeSet(keys));
  }

  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice, /*useSequentialIteration*/ false);

  while (it.valid()) {
    auto kv = *it;
    SDB_ASSERT(kv.key.isString());
    auto key = kv.key.stringViewUnchecked();
    if (absl::c_linear_search(keys, key)) {
      b.add(key, kv.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::keep(Slice slice,
                         const absl::flat_hash_set<std::string>& keys) {
  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice, /*useSequentialIteration*/ false);

  while (it.valid()) {
    auto kv = *it;
    SDB_ASSERT(kv.key.isString());
    auto key = kv.key.stringViewUnchecked();
    if (keys.contains(key)) {
      b.add(key, kv.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::remove(Slice slice, const std::vector<std::string>& keys) {
  // check if there are so many keys that we want to use the hash-based version
  // cut-off values are arbitrary...
  if (keys.size() >= 4 && slice.length() > 10) {
    return remove(slice, MakeSet(keys));
  }

  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice, /*useSequentialIteration*/ false);

  while (it.valid()) {
    auto kv = *it;
    auto key = kv.key.stringView();
    if (!absl::c_linear_search(keys, key)) {
      b.add(key, kv.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::remove(Slice slice,
                           const absl::flat_hash_set<std::string>& keys) {
  Builder b;
  b.add(Value(ValueType::Object));

  ObjectIterator it(slice, /*useSequentialIteration*/ false);

  while (it.valid()) {
    auto kv = *it;
    auto key = kv.key.stringView();
    if (!keys.contains(key)) {
      b.add(key, kv.value());
    }
    it.next();
  }

  b.close();
  return b;
}

Builder Collection::merge(Slice left, Slice right, bool merge_values,
                          bool null_means_remove) {
  if (!left.isObject() || !right.isObject()) {
    throw Exception(Exception::kInvalidValueType, "Expecting type Object");
  }

  Builder b;
  Collection::merge(b, left, right, merge_values, null_means_remove);
  return b;
}

Builder& Collection::merge(Builder& builder, Slice left, Slice right,
                           bool merge_values, bool null_means_remove) {
  if (!left.isObject() || !right.isObject()) {
    throw Exception(Exception::kInvalidValueType, "Expecting type Object");
  }

  builder.add(Value(ValueType::Object));

  absl::flat_hash_map<std::string_view, Slice> right_values;
  {
    ObjectIterator it(right, /*useSequentialIteration*/ true);
    while (it.valid()) {
      auto [k, v] = *it;
      SDB_ASSERT(k.isString());
      right_values.emplace(k.stringViewUnchecked(), v);
      it.next();
    }
  }

  {
    ObjectIterator it(left, /*useSequentialIteration*/ false);

    while (it.valid()) {
      auto current = (*it);
      auto key = current.key.stringView();
      auto v = current.value();
      auto found = right_values.find(key);

      if (found == right_values.end()) {
        // use left value
        builder.add(key, v);
      } else if (merge_values && v.isObject() && (*found).second.isObject()) {
        // merge both values
        auto& value = (*found).second;
        if (!null_means_remove || (!value.isNone() && !value.isNull())) {
          builder.add(key);
          Collection::merge(builder, v, value, true, null_means_remove);
        }
        // clear the value in the map so its not added again
        (*found).second = Slice();
      } else {
        // use right value
        auto& value = (*found).second;
        if (!null_means_remove || (!value.isNone() && !value.isNull())) {
          builder.add(key, value);
        }
        // clear the value in the map so its not added again
        (*found).second = Slice();
      }
      it.next();
    }
  }

  // add remaining values that were only in right
  for (auto& it : right_values) {
    auto& s = it.second;
    if (s.isNone()) {
      continue;
    }
    if (null_means_remove && s.isNull()) {
      continue;
    }
    builder.add(std::move(it.first), s);
  }

  builder.close();
  return builder;
}

template<Collection::VisitationOrder Order>
static bool DoVisit(Slice slice,
                    const std::function<bool(Slice key, Slice value)>& func);

template<Collection::VisitationOrder Order>
static bool VisitObject(
  Slice value, const std::function<bool(Slice key, Slice value)>& func) {
  ObjectIterator it(value, /*useSequentialIteration*/ false);

  while (it.valid()) {
    auto [k, v] = *it;
    const bool is_compound = (v.isObject() || v.isArray());

    if (is_compound && Order == Collection::kPreOrder) {
      if (!DoVisit<Order>(v, func)) {
        return false;
      }
    }

    if (!func(k, v)) {
      return false;
    }

    if (is_compound && Order == Collection::kPostOrder) {
      if (!DoVisit<Order>(v, func)) {
        return false;
      }
    }

    it.next();
  }
  return true;
}

template<Collection::VisitationOrder Order>
static bool VisitArray(
  Slice value, const std::function<bool(Slice key, Slice value)>& func) {
  ArrayIterator it(value);

  while (it.valid()) {
    // sub-object?
    Slice v = it.value();
    const bool is_compound = (v.isObject() || v.isArray());

    if (is_compound && Order == Collection::kPreOrder) {
      if (!DoVisit<Order>(v, func)) {
        return false;
      }
    }

    if (!func(Slice(), v)) {
      return false;
    }

    if (is_compound && Order == Collection::kPostOrder) {
      if (!DoVisit<Order>(v, func)) {
        return false;
      }
    }

    it.next();
  }

  return true;
}

template<Collection::VisitationOrder Order>
static bool DoVisit(Slice slice,
                    const std::function<bool(Slice key, Slice value)>& func) {
  if (slice.isObject()) {
    return VisitObject<Order>(slice, func);
  }
  if (slice.isArray()) {
    return VisitArray<Order>(slice, func);
  }

  throw Exception(Exception::kInvalidValueType,
                  "Expecting type Object or Array");
}

void Collection::visitRecursive(Slice slice, Collection::VisitationOrder order,
                                const std::function<bool(Slice, Slice)>& func) {
  if (order == Collection::kPreOrder) {
    DoVisit<Collection::kPreOrder>(slice, func);
  } else {
    DoVisit<Collection::kPostOrder>(slice, func);
  }
}

Builder Collection::sort(Slice array,
                         std::function<bool(Slice, Slice)> lessthan) {
  if (!array.isArray()) {
    throw Exception(Exception::kInvalidValueType, "Expecting type Array");
  }
  std::vector<Slice> sub_values;
  ValueLength len = array.length();
  sub_values.reserve(CheckOverflow(len));
  for (ValueLength i = 0; i < len; i++) {
    sub_values.push_back(array.at(i));
  }
  std::sort(sub_values.begin(), sub_values.end(), lessthan);
  Builder b;
  b.openArray();
  for (const auto& s : sub_values) {
    b.add(s);
  }
  b.close();
  return b;
}
