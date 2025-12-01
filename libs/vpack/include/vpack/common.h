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

// for endianess detection
#include <bit>
#include <cstdint>
// for size_t:
#include <absl/base/internal/endian.h>

#include <cstring>
#include <limits>
#include <type_traits>

#include "basics/exceptions.h"
#include "basics/number_utils.h"

// check for environment type (32 or 64 bit)
// if the environment type cannot be determined reliably, then this will
// abort compilation. this will abort on systems that neither have 32 bit
// nor 64 bit pointers!
#if INTPTR_MAX == INT32_MAX
#define VPACK_32BIT

#elif INTPTR_MAX == INT64_MAX
#define VPACK_64BIT

#else
#error "Could not determine environment type (32 or 64 bits)"
#endif

// attribute used to tag potentially unused functions (used mostly in tests/)
#ifdef __GNUC__
#define VPACK_UNUSED __attribute__((unused))
#else
#define VPACK_UNUSED /* unused */
#endif

// attribute used to force inlining of functions
#if defined(__GNUC__) || defined(__clang__)
#define VPACK_FORCE_INLINE inline __attribute__((__always_inline__))
#elif _WIN32
#define VPACK_FORCE_INLINE __forceinline
#endif

// always define wy hash function, in addition to other configured
// hash function
#include "vpack/wyhash.h"

// the default secret parameters
// the default secret parameters
inline constexpr uint64_t kWyp[4] = {
  0x2d358dccaa6c78a5ull,
  0x8bb84b93962eacc9ull,
  0x4b33a62ed433d4a3ull,
  0x4d5a2da51de1aa47ull,
};

#define VPACK_HASH(mem, size, seed) wyhash(mem, size, seed, kWyp)

namespace vpack {

template<typename Ptr>
bool IsOwning(const Ptr& ptr) {
  return ptr.owner_before(Ptr{}) || Ptr{}.owner_before(ptr);
}

template<typename T>
VPACK_FORCE_INLINE T HostToLittle(T in) {
  return absl::little_endian::FromHost(in);
}

template<typename T>
VPACK_FORCE_INLINE T LittleToHost(T in) {
  return absl::little_endian::ToHost(in);
}

// unified size type for VPack, can be used on 32 and 64 bit
// though no VPack values can exceed the bounds of 32 bit on a 32 bit OS
typedef uint64_t ValueLength;

#ifndef VPACK_64BIT
// check if the length is beyond the size of a SIZE_MAX on this platform
size_t checkOverflow(ValueLength);
#else
// on a 64 bit platform, the following function is probably a no-op
VPACK_FORCE_INLINE constexpr size_t CheckOverflow(ValueLength length) noexcept {
  return static_cast<size_t>(length);
}
#endif

// calculate the length of a variable length integer in unsigned LEB128 format
inline ValueLength GetVariableValueLength(ValueLength value) noexcept {
#if (defined(__x86__) || defined(__x86_64__) || defined(_M_IX86) || \
     defined(_M_X64)) &&                                            \
  !(defined(__LZCNT__) || defined(__AVX2__))
  // Explicit OR 0x1 to avoid calling std::countl_zero(0), which
  // requires a branch to check for on platforms without a clz instruction.
  uint32_t log2value = (std::numeric_limits<ValueLength>::digits - 1) -
                       std::countl_zero(value | 0x1);
  return static_cast<ValueLength>((log2value * 9 + (64 + 9)) / 64);
#else
  uint32_t clz = std::countl_zero(value);
  return static_cast<ValueLength>(
    ((std::numeric_limits<ValueLength>::digits * 9 + 64) - (clz * 9)) / 64);
#endif
}

// read a variable length integer in unsigned LEB128 format
template<bool Reverse>
inline ValueLength ReadVariableValueLength(const uint8_t* source) noexcept {
  ValueLength len = 0;
  uint8_t v;
  ValueLength p = 0;
  do {
    v = *source;
    len += static_cast<ValueLength>(v & 0x7fU) << p;
    p += 7;
    if constexpr (Reverse) {
      --source;
    } else {
      ++source;
    }
  } while (v & 0x80U);
  return len;
}

// store a variable length integer in unsigned LEB128 format
template<bool Reverse>
inline void StoreVariableValueLength(uint8_t* dst, ValueLength value) noexcept {
  SDB_ASSERT(value > 0);

  if (Reverse) {
    while (value >= 0x80U) {
      *dst-- = static_cast<uint8_t>(value | 0x80U);
      value >>= 7;
    }
    *dst = static_cast<uint8_t>(value & 0x7fU);
  } else {
    while (value >= 0x80U) {
      *dst++ = static_cast<uint8_t>(value | 0x80U);
      value >>= 7;
    }
    *dst = static_cast<uint8_t>(value & 0x7fU);
  }
}

// read an unsigned little endian integer value of the
// specified length, starting at the specified byte offset
template<typename T, uint8_t Len>
inline T ReadIntegerFixed(const uint8_t* start) noexcept {
  static_assert(Len > 0);
  static_assert(Len <= sizeof(T));
  if constexpr (std::is_unsigned_v<T>) {
    T v{};
    memcpy(&v, start, Len);
    return absl::little_endian::ToHost(v);
  } else if constexpr (Len == 1) {
    const auto v = ReadIntegerFixed<uint8_t, Len>(start);
    return std::bit_cast<int8_t>(v);
  } else if constexpr (Len == 2) {
    const auto v = ReadIntegerFixed<uint16_t, Len>(start);
    return std::bit_cast<int16_t>(v);
  } else if constexpr (Len == 4) {
    const auto v = ReadIntegerFixed<uint32_t, Len>(start);
    return std::bit_cast<int32_t>(v);
  } else if constexpr (Len == 8) {
    const auto v = ReadIntegerFixed<uint64_t, Len>(start);
    return std::bit_cast<int64_t>(v);
  } else {
    const auto v = ReadIntegerFixed<std::make_unsigned_t<T>, Len>(start);
    return static_cast<T>(sdb::ZigZagDecode64(v));
  }
}

// read an unsigned little endian integer value of the
// specified, non-0 length, starting at the specified byte offset
template<typename T>
inline T ReadIntegerNonEmpty(const uint8_t* start,
                             ValueLength length) noexcept {
  SDB_ASSERT(length > 0);
  SDB_ASSERT(length <= sizeof(T));
  SDB_ASSERT(length <= 8);
  switch (length) {
    case 1:
      return ReadIntegerFixed<T, 1>(start);
    case 2:
      return ReadIntegerFixed<T, 2>(start);
    case 3:
      return ReadIntegerFixed<T, 3>(start);
    case 4:
      return ReadIntegerFixed<T, 4>(start);
    case 5:
      return ReadIntegerFixed<T, 5>(start);
    case 6:
      return ReadIntegerFixed<T, 6>(start);
    case 7:
      return ReadIntegerFixed<T, 7>(start);
    case 8:
      return ReadIntegerFixed<T, 8>(start);
  }
  SDB_ASSERT(false);
  return 0;
}

}  // namespace vpack
