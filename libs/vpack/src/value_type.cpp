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

#include "vpack/value_type.h"

#include <ostream>

namespace vpack {

std::string_view ValueTypeName(ValueType type) {
  switch (type) {
    case ValueType::None:
      return "none";
    case ValueType::Null:
      return "null";
    case ValueType::Bool:
      return "bool";
    case ValueType::Array:
      return "array";
    case ValueType::Object:
      return "object";
    case ValueType::Double:
      return "double";
    case ValueType::Int:
      return "int";
    case ValueType::UInt:
      return "uint";
    case ValueType::SmallInt:
      return "smallint";
    case ValueType::String:
      return "string";
  }

  return "unknown";
}

ValueType ValueTypeGroup(ValueType type) {
  // numbers are all the same, upcast them to double
  if (type == ValueType::Double || type == ValueType::Int ||
      type == ValueType::UInt || type == ValueType::SmallInt) {
    return ValueType::Double;
  }
  return type;
}

}  // namespace vpack

std::ostream& operator<<(std::ostream& stream, vpack::ValueType type) {
  stream << ValueTypeName(type);
  return stream;
}
