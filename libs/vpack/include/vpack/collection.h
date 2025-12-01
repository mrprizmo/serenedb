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

#include <absl/container/flat_hash_set.h>

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "vpack/builder.h"
#include "vpack/common.h"
#include "vpack/iterator.h"
#include "vpack/slice.h"

namespace vpack {

class Collection {
 public:
  enum VisitationOrder { kPreOrder = 1, kPostOrder = 2 };

  // indicator for "element not found" in indexOf() method
  static constexpr ValueLength kNotFound = UINT64_MAX;

  using Predicate = std::function<bool(Slice, ValueLength)>;

  Collection() = delete;
  Collection(const Collection&) = delete;
  Collection& operator=(const Collection&) = delete;

  static Builder& appendArray(Builder& builder, Slice left);

  static void forEach(Slice slice, const Predicate& predicate);

  static void forEach(const Slice* slice, const Predicate& predicate) {
    return forEach(*slice, predicate);
  }

  static Builder filter(Slice slice, const Predicate& predicate);

  static Builder filter(const Slice* slice, const Predicate& predicate) {
    return filter(*slice, predicate);
  }

  static Slice find(Slice slice, const Predicate& predicate);

  static Slice find(const Slice* slice, const Predicate& predicate) {
    return find(*slice, predicate);
  }

  static bool contains(Slice slice, const Predicate& predicate);

  static bool contains(const Slice* slice, const Predicate& predicate) {
    return contains(*slice, predicate);
  }

  static bool contains(Slice slice, Slice other);

  static bool contains(const Slice* slice, Slice other) {
    return contains(*slice, other);
  }

  static ValueLength indexOf(Slice slice, Slice other);

  static ValueLength indexOf(const Slice* slice, Slice other) {
    return indexOf(*slice, other);
  }

  static bool all(Slice slice, const Predicate& predicate);

  static bool all(const Slice* slice, const Predicate& predicate) {
    return all(*slice, predicate);
  }

  static bool any(Slice slice, const Predicate& predicate);

  static bool any(const Slice* slice, const Predicate& predicate) {
    return any(*slice, predicate);
  }

  static std::vector<std::string> keys(Slice slice);

  static std::vector<std::string> keys(const Slice* slice) {
    return keys(*slice);
  }

  template<typename T>
  static void keys(Slice slice, T& result) {
    ObjectIterator it(slice, /*useSequentialIteration*/ true);

    while (it.valid()) {
      std::string_view sv = (*it).key.stringView();
      result.emplace(sv.data(), sv.size());
      it.next();
    }
  }

  template<typename T>
  static void keys(const Slice* slice, T& result) {
    return keys(*slice, result);
  }

  static void keys(Slice slice, std::vector<std::string>& result) {
    // pre-allocate result vector
    ObjectIterator it(slice, /*useSequentialIteration*/ false);
    result.reserve(CheckOverflow(it.size()));

    while (it.valid()) {
      std::string_view sv = (*it).key.stringView();
      result.emplace_back(sv.data(), sv.size());
      it.next();
    }
  }

  template<typename T>
  static void unorderedKeys(Slice slice, T& result) {
    ObjectIterator it(slice, /*useSequentialIteration*/ true);

    while (it.valid()) {
      std::string_view sv = (*it).key.stringView();
      result.emplace(sv.data(), sv.size());
      it.next();
    }
  }

  template<typename T>
  static void unorderedKeys(const Slice* slice, T& result) {
    return unorderedKeys(*slice, result);
  }

  static Builder extract(Slice slice, int64_t from, int64_t to = INT64_MAX);
  static Builder extract(const Slice* slice, int64_t from,
                         int64_t to = INT64_MAX) {
    return extract(*slice, from, to);
  }

  static Builder concat(Slice slice1, Slice slice2);
  static Builder concat(const Slice* slice1, const Slice* slice2) {
    return concat(*slice1, *slice2);
  }

  static Builder values(Slice slice);
  static Builder values(const Slice* slice) { return values(*slice); }

  static Builder keep(Slice slice, const std::vector<std::string>& keys);
  static Builder keep(Slice slice,
                      const absl::flat_hash_set<std::string>& keys);

  static Builder keep(const Slice* slice,
                      const std::vector<std::string>& keys) {
    return keep(*slice, keys);
  }

  static Builder keep(const Slice* slice,
                      const absl::flat_hash_set<std::string>& keys) {
    return keep(*slice, keys);
  }

  static Builder remove(Slice slice, const std::vector<std::string>& keys);

  static Builder remove(Slice slice,
                        const absl::flat_hash_set<std::string>& keys);

  static Builder remove(const Slice* slice,
                        const std::vector<std::string>& keys) {
    return remove(*slice, keys);
  }

  static Builder remove(const Slice* slice,
                        const absl::flat_hash_set<std::string>& keys) {
    return remove(*slice, keys);
  }

  static Builder merge(Slice left, Slice right, bool merge_values,
                       bool null_means_remove = false);

  static Builder merge(const Slice* left, const Slice* right, bool merge_values,
                       bool null_means_remove = false) {
    return merge(*left, *right, merge_values, null_means_remove);
  }
  static Builder& merge(Builder& builder, Slice left, Slice right,
                        bool merge_values, bool null_means_remove = false);

  static void visitRecursive(Slice slice, VisitationOrder order,
                             const std::function<bool(Slice, Slice)>& func);

  static void visitRecursive(const Slice* slice, VisitationOrder order,
                             const std::function<bool(Slice, Slice)>& func) {
    visitRecursive(*slice, order, func);
  }

  static Builder sort(Slice array, std::function<bool(Slice, Slice)> lessthan);
};

}  // namespace vpack
