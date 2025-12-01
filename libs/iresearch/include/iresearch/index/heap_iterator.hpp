////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>
#include <vector>

#include "basics/shared.hpp"
#include "iterators.hpp"

namespace irs {

template<typename Context>
class ExternalMergeIterator {
  using Value = typename Context::Value;

 public:
  template<typename... Args>
  explicit ExternalMergeIterator(Args&&... args)
    : _ctx{std::forward<Args>(args)...} {}

  bool Initilized() const noexcept { return !_tree.empty(); }

  void Reset(std::span<Value> values) noexcept {
    _size = 0;
    _tree.clear();
    _values = values;
  }

  bool Next() {
    if (_size == 0) [[unlikely]] {
      return LazyReset();
    }
    auto& lead = Lead();
    auto position = _values.size() + (&lead - _values.data());
    if (!_ctx(lead)) [[unlikely]] {
      if (--_size == 0) {
        return false;
      }
      _tree[position] = nullptr;
    }
    while ((position /= 2) != 0) {
      _tree[position] = Compute(position);
    }
    return true;
  }

  IRS_FORCE_INLINE Value& Lead() const noexcept {
    SDB_ASSERT(_size > 0);
    SDB_ASSERT(_tree.size() > 1);
    SDB_ASSERT(_tree[1] != nullptr);
    return *_tree[1];
  }

  size_t Size() const noexcept { return _size; }

 private:
  IRS_FORCE_INLINE Value* Compute(size_t position) {
    auto* lhs = _tree[2 * position];
    auto* rhs = _tree[2 * position + 1];
    if ((lhs != nullptr) & (rhs != nullptr)) {
      return _ctx(*lhs, *rhs) ? lhs : rhs;
    }
    return reinterpret_cast<Value*>(reinterpret_cast<uintptr_t>(lhs) |
                                    reinterpret_cast<uintptr_t>(rhs));
  }

  IRS_NO_INLINE bool LazyReset() {
    SDB_ASSERT(_size == 0);
    if (!_tree.empty()) {
      return false;
    }
    _size = _values.size();
    if (_size == 0) {
      return false;
    }
    // bottom-up min segment tree construction
    _tree.resize(_size * 2);
    // init leafs
    auto* values = _values.data();
    for (auto it = _tree.begin() + _size, end = _tree.end(); it != end; ++it) {
      if (_ctx(*values)) {
        *it = values;
      } else {
        *it = nullptr;
        --_size;
      }
      ++values;
    }
    // compute tree
    for (auto position = _values.size() - 1; position != 0; --position) {
      _tree[position] = Compute(position);
    }
    // stub node for faster compute
    _tree[0] = nullptr;
    return _size != 0;
  }

  [[no_unique_address]] Context _ctx;
  size_t _size{0};
  std::vector<Value*> _tree;
  std::span<Value> _values;
};

}  // namespace irs
