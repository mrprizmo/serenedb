////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>

#include "basics/arithmetic.h"
#include "basics/number_utils.h"
#include "basics/string_utils.h"

namespace sdb::options::units_helper {

std::pair<std::string_view, uint64_t> ExtractSuffix(std::string_view value);

// turns a number string with an optional unit suffix into a numeric
// value. will throw std::out_of_range for invalid values.
// it is required that any input value to this function is stripped of
// any leading and trailing whitespace characters. otherwise the
// function will throw an exception
template<typename T, typename Internal>
T ParseNumberWithUnit(std::string_view value, T base = 1) {
  // handle unit suffixes
  Internal m = 1;
  auto [suffix, multiplier] = ExtractSuffix(value);
  if (!suffix.empty()) {
    m = static_cast<Internal>(multiplier);
  }

  // handle % suffix
  Internal d = 1;
  if (suffix.empty() && value.ends_with('%')) {
    suffix = "%";
    m = static_cast<Internal>(base);
    d = 100;
  }

  bool valid = true;
  auto v = number_utils::Atoi<T>(
    value.data(), value.data() + value.size() - suffix.size(), valid);
  if (!valid) {
    throw std::out_of_range(std::string{value});
  }
  if (IsUnsafeMultiplication(static_cast<Internal>(v), m)) {
    throw std::out_of_range(std::string{value});
  }
  Internal r = static_cast<Internal>(v) * m;
  if (r < std::numeric_limits<T>::min() || r > std::numeric_limits<T>::max()) {
    throw std::out_of_range(std::string{value});
  }
  return static_cast<T>(v * m / d);
}

// convert a string into a number, base version for signed and unsigned integer
// types
template<typename T>
T ToNumber(std::string value, T base = 1) {
  // replace leading spaces, replace trailing spaces & comments
  value = basics::string_utils::RemoveWhitespaceAndComments(value);

  if constexpr (std::is_signed_v<T>) {
    return units_helper::ParseNumberWithUnit<T, int64_t>(value, base);
  } else {
    return units_helper::ParseNumberWithUnit<T, uint64_t>(value, base);
  }
}

// convert a string into a number, version for double values
template<>
inline double ToNumber<double>(std::string value, double /*base*/) {
  // replace leading spaces, replace trailing spaces & comments
  return std::stod(basics::string_utils::RemoveWhitespaceAndComments(value));
}

// convert a string into another type, specialized version for numbers
template<typename T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type FromString(
  const std::string& value) {
  return ToNumber<T>(value, static_cast<T>(1));
}

// convert a string into another type, specialized version for string -> string
template<typename T>
typename std::enable_if<std::is_same<T, std::string>::value, T>::type
FromString(const std::string& value) {
  return value;
}

}  // namespace sdb::options::units_helper
