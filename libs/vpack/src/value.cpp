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

#include "vpack/value.h"

#include "vpack/exception.h"

using namespace vpack;

// creates a Value with the specified type Array or Object
Value::Value(ValueType t, bool allow_unindexed)
  : _value_type(t), _value(allow_unindexed) {
  // we use the boolean part to store the allowUnindexed value
  if (allow_unindexed &&
      (_value_type != ValueType::Array && _value_type != ValueType::Object)) {
    throw Exception(Exception::kInvalidValueType, "Expecting compound type");
  }
}

bool Value::unindexed() const {
  if (_value_type != ValueType::Array && _value_type != ValueType::Object) {
    throw Exception(Exception::kInvalidValueType, "Expecting compound type");
  }
  return _value;
}
