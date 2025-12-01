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
#include <type_traits>

#include "basics/common.h"
#include "basics/system-compiler.h"

namespace sdb {
namespace number_utils {

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a negative number value of type T,
// without validation of the input string - use this only for trusted input!
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// there is no validation of the input string, and overflow or underflow
// of the result value will not be detected.
// this function will not modify errno.
template<typename T>
inline T AtoiNegativeUnchecked(const char* p, const char* e) noexcept {
  T result = 0;
  while (p != e) {
    result = (result * 10) - (*(p++) - '0');
  }
  return result;
}

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a positive number value of type T,
// without validation of the input string - use this only for trusted input!
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// there is no validation of the input string, and overflow or underflow
// of the result value will not be detected.
// this function will not modify errno.
template<typename T>
inline T AtoiPositiveUnchecked(const char* p, const char* e) noexcept {
  T result = 0;
  while (p != e) {
    result = (result * 10) + (*(p++) - '0');
  }
  return result;
}

// function to convert the string value between p
// (inclusive) and e (exclusive) into a number value of type T, without
// validation of the input string - use this only for trusted input!
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'. an
// optional '+' or '-' sign is allowed too.
// there is no validation of the input string, and overflow or underflow
// of the result value will not be detected.
// this function will not modify errno.
template<typename T>
inline T AtoiUnchecked(const char* p, const char* e) noexcept {
  if (p == e) [[unlikely]] {
    return T();
  }

  if (*p == '-') {
    if (!std::is_signed<T>::value) {
      return T();
    }
    return AtoiNegativeUnchecked<T>(++p, e);
  }
  if (*p == '+') [[unlikely]] {
    ++p;
  }

  return AtoiPositiveUnchecked<T>(p, e);
}

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a negative number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// if any other character is found, the output parameter "valid" will
// be set to false. if the parsed value is less than what type T can
// store without truncation, "valid" will also be set to false.
// this function will not modify errno.
template<typename T>
inline T AtoiNegative(const char* p, const char* e, bool& valid) noexcept {
  if (p == e) [[unlikely]] {
    valid = false;
    return T();
  }

  constexpr T kCutoff = (std::numeric_limits<T>::min)() / 10;
  constexpr char kCutlim = -((std::numeric_limits<T>::min)() % 10);
  T result = 0;

  do {
    char c = *p;
    // we expect only '0' to '9'. everything else is unexpected
    if (c < '0' || c > '9') [[unlikely]] {
      valid = false;
      return result;
    }

    c -= '0';
    // we expect the bulk of values to not hit the bounds restrictions
    if (result < kCutoff || (result == kCutoff && c > kCutlim)) [[unlikely]] {
      valid = false;
      return result;
    }
    result *= 10;
    result -= c;
  } while (++p < e);

  valid = true;
  return result;
}

// low-level worker function to convert the string value between p
// (inclusive) and e (exclusive) into a positive number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'.
// if any other character is found, the output parameter "valid" will
// be set to false. if the parsed value is greater than what type T can
// store without truncation, "valid" will also be set to false.
// this function will not modify errno.
template<typename T>
inline T AtoiPositive(const char* p, const char* e, bool& valid) noexcept {
  if (p == e) [[unlikely]] {
    valid = false;
    return T();
  }

  constexpr T kCutoff = (std::numeric_limits<T>::max)() / 10;
  constexpr char kCutlim = (std::numeric_limits<T>::max)() % 10;
  T result = 0;

  do {
    char c = *p;

    // we expect only '0' to '9'. everything else is unexpected
    if (c < '0' || c > '9') [[unlikely]] {
      valid = false;
      return result;
    }

    c -= '0';
    // we expect the bulk of values to not hit the bounds restrictions
    if (result > kCutoff || (result == kCutoff && c > kCutlim)) [[unlikely]] {
      valid = false;
      return result;
    }
    result *= 10;
    result += static_cast<T>(c);
  } while (++p < e);

  valid = true;
  return result;
}

// function to convert the string value between p
// (inclusive) and e (exclusive) into a number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'. an
// optional '+' or '-' sign is allowed too.
// if any other character is found, the output parameter "valid" will
// be set to false. if the parsed value is less or greater than what
// type T can store without truncation, "valid" will also be set to
// false.
// this function will not modify errno.
template<typename T>
inline typename std::enable_if<std::is_signed<T>::value, T>::type Atoi(
  const char* p, const char* e, bool& valid) noexcept {
  if (p == e) [[unlikely]] {
    valid = false;
    return T();
  }

  if (*p == '-') {
    return AtoiNegative<T>(++p, e, valid);
  }
  if (*p == '+') [[unlikely]] {
    ++p;
  }

  return AtoiPositive<T>(p, e, valid);
}

template<typename T>
inline typename std::enable_if<std::is_unsigned<T>::value, T>::type Atoi(
  const char* p, const char* e, bool& valid) noexcept {
  if (p == e) [[unlikely]] {
    valid = false;
    return T();
  }

  if (*p == '-') {
    valid = false;
    return T();
  }
  if (*p == '+') [[unlikely]] {
    ++p;
  }

  return AtoiPositive<T>(p, e, valid);
}

// function to convert the string value between p
// (inclusive) and e (exclusive) into a number value of type T
//
// the input string will always be interpreted as a base-10 number.
// expects the input string to contain only the digits '0' to '9'. an
// optional '+' or '-' sign is allowed too.
// if any other character is found, the result will be set to 0.
// if the parsed value is less or greater than what type T can store
// without truncation, return result will also be set to 0.
// this function will not modify errno.
template<typename T>
inline T AtoiZero(const char* p, const char* e) noexcept {
  bool valid;
  T result = Atoi<T>(p, e, valid);
  return valid ? result : 0;
}

template<typename T>
constexpr std::enable_if_t<std::is_integral<T>::value, bool> IsPowerOf2(T n) {
  return n > 0 && (n & (n - 1)) == 0;
}

/// calculate the integer log2 value for the given input
/// the result is undefined when calling this with a value of 0!
uint32_t Log2(uint32_t value) noexcept;

template<typename From, typename To = double>
consteval To Min() {
  static_assert(std::is_integral_v<From>);
  if constexpr (std::is_integral_v<To>) {
    static_assert(std::is_signed_v<To>);
    static_assert(sizeof(From) <= sizeof(To));
    return static_cast<To>(std::numeric_limits<From>::min());
  } else if constexpr (std::is_signed_v<From>) {
    return static_cast<To>(std::numeric_limits<From>::min());
  } else {
    return {};
  }
}

template<typename From, typename To = double>
consteval double Max() {
  static_assert(std::is_integral_v<From>);
  if constexpr (std::is_integral_v<To>) {
    static_assert(sizeof(From) <= sizeof(To));
    if constexpr (sizeof(From) == sizeof(To) && std::is_signed_v<To>) {
      return std::numeric_limits<To>::max();
    } else {
      return static_cast<To>(std::numeric_limits<From>::max());
    }
  } else if constexpr (std::is_signed_v<From>) {
    return -static_cast<To>(std::numeric_limits<From>::min());
  } else if (sizeof(From) <= 4) {
    return static_cast<To>(std::numeric_limits<From>::max()) + 1.0;
  } else {
    return static_cast<To>(std::numeric_limits<From>::max());
  }
}

}  // namespace number_utils

constexpr uint32_t ZigZagEncode32(int32_t n) noexcept {
  // right shift must be arithmetic
  // left shift must be unsigned because of overflow
  return (static_cast<uint32_t>(n) << 1) ^ static_cast<uint32_t>(n >> 31);
}

constexpr int32_t ZigZagDecode32(uint32_t n) noexcept {
  // using unsigned types prevent undefined behavior
  return static_cast<int32_t>((n >> 1) ^ (~(n & 1) + 1));
}

constexpr uint64_t ZigZagEncode64(int64_t n) noexcept {
  // right shift must be arithmetic
  // left shift must be unsigned because of overflow
  return (static_cast<uint64_t>(n) << 1) ^ static_cast<uint64_t>(n >> 63);
}

constexpr int64_t ZigZagDecode64(uint64_t n) noexcept {
  // using unsigned types prevent undefined behavior
  return static_cast<int64_t>((n >> 1) ^ (~(n & 1) + 1));
}

static_assert(number_utils::Min<int8_t>() == -128.0);  // -2^7
static_assert(number_utils::Min<uint8_t>() == 0.0);    //  0
static_assert(number_utils::Max<int8_t>() == 128.0);   //  2^7
static_assert(number_utils::Max<uint8_t>() == 256.0);  //  2^8

static_assert(number_utils::Min<int16_t>() == -32768.0);  // -2^15
static_assert(number_utils::Min<uint16_t>() == 0.0);      //  0
static_assert(number_utils::Max<int16_t>() == 32768.0);   //  2^15
static_assert(number_utils::Max<uint16_t>() == 65536.0);  //  2^16

static_assert(number_utils::Min<int32_t>() == -2147483648.0);  // -2^31
static_assert(number_utils::Min<uint32_t>() == 0.0);           //  0
static_assert(number_utils::Max<int32_t>() == 2147483648.0);   //  2^31
static_assert(number_utils::Max<uint32_t>() == 4294967296.0);  //  2^32

static_assert(number_utils::Min<int64_t>() == -9223372036854775808.0);  // -2^63
static_assert(number_utils::Min<uint64_t>() == 0.0);                    //  0
static_assert(number_utils::Max<int64_t>() == 9223372036854775808.0);   //  2^63
static_assert(number_utils::Max<uint64_t>() ==
              18446744073709551616.0);  //  2^64

}  // namespace sdb
