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
#include <string>
#include <string_view>

#include "vpack/common.h"
#include "vpack/value_type.h"

namespace vpack {

class Value {
  friend class Builder;

  const ValueType _value_type;
  const bool _value;

 public:
  // creates a Value with the specified type Array or Object
  explicit Value(ValueType t, bool allow_unindexed = false);

  ValueType valueType() const noexcept { return _value_type; }

  // whether or not the underlying Array/Object can be unindexed.
  // it is only allowed to call this for Array/Object types!
  bool unindexed() const;
};

// TODO(mbkkt) Make it concept to avoid size() + 2 virtual calls
struct IStringFromParts {
  virtual ~IStringFromParts() = default;

  virtual size_t numberOfParts() const = 0;

  virtual size_t totalLength() const = 0;

  virtual std::string_view operator()(size_t index) const = 0;
};

}  // namespace vpack
