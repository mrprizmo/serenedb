////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "basics/assert.h"
#include "iresearch/utils/attribute_provider.hpp"
#include "iresearch/utils/attributes.hpp"

namespace irs {

// Represents an estimated cost of the query execution.
class CostAttr final : public Attribute {
 public:
  using cost_t = uint64_t;                               // NOLINT
  using cost_f = absl::AnyInvocable<cost_t() noexcept>;  // NOLINT

  static_assert(std::is_nothrow_move_constructible_v<cost_f>);
  static_assert(std::is_nothrow_move_assignable_v<cost_f>);

  static constexpr std::string_view type_name() noexcept { return "cost"; }

  static constexpr cost_t kMax = std::numeric_limits<cost_t>::max();

  CostAttr() = default;

  explicit CostAttr(cost_t value) noexcept : _value{value} {}

  explicit CostAttr(cost_f&& func) noexcept : _func{std::move(func)} {
    SDB_ASSERT(_func);
  }

  // Returns a value of the "cost" attribute in the specified "src"
  // collection, or "def" value if there is no "cost" attribute in "src".
  template<typename Provider>
  static cost_t extract(const Provider& src, cost_t def = kMax) noexcept {
    if (auto* attr = irs::get<irs::CostAttr>(src); attr) {
      return attr->estimate();
    } else {
      return def;
    }
  }

  // Sets the estimation value.
  void reset(cost_t value) noexcept {
    _value = value;
    _func = nullptr;
  }

  // Sets the estimation rule.
  void reset(cost_f&& func) noexcept {
    _func = std::move(func);
    SDB_ASSERT(_func);
  }

  // Estimate the query according to the provided estimation function.
  // Return estimated cost.
  cost_t estimate() const noexcept {
    if (_func) [[unlikely]] {
      _value = _func();
      _func = nullptr;
    }
    return _value;
  }

 private:
  mutable cost_f _func;  // evaluation function
  mutable cost_t _value{0};
};

}  // namespace irs
