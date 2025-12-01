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

#include "iresearch/utils/string.hpp"

namespace irs::numeric_utils {

bytes_view Mini64();
bytes_view Maxi64();

uint64_t Decode64(const byte_type* out);
size_t Encode64(uint64_t value, byte_type* out, size_t shift = 0);
bytes_view Minu64();
bytes_view Maxu64();

bytes_view Mini32();
bytes_view Maxi32();

uint32_t Decode32(const byte_type* out);
size_t Encode32(uint32_t value, byte_type* out, size_t shift = 0);
bytes_view Minu32();
bytes_view Maxu32();

size_t Encodef32(uint32_t value, byte_type* out, size_t shift = 0);
uint32_t Decodef32(const byte_type* out);
int32_t Ftoi32(float_t value);
float_t I32tof(int32_t value);
bytes_view Minf32();
bytes_view Maxf32();
bytes_view Finf32();
bytes_view Nfinf32();

size_t Encoded64(uint64_t value, byte_type* out, size_t shift = 0);
uint64_t Decoded64(const byte_type* out);
int64_t Dtoi64(double_t value);
double_t I64tod(int64_t value);
bytes_view Mind64();
bytes_view Maxd64();
bytes_view Dinf64();
bytes_view Ndinf64();

template<typename T>
struct numeric_traits;

template<>
struct numeric_traits<int32_t> {
  typedef int32_t integral_t;
  static bytes_view min() { return Mini32(); }
  static bytes_view max() { return Maxi32(); }
  inline static integral_t integral(integral_t value) { return value; }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return Encode32(value, out, offset);
  }
  static integral_t decode(const byte_type* in) { return Decode32(in); }
};

template<>
struct numeric_traits<uint32_t> {
  typedef uint32_t integral_t;
  static integral_t decode(const byte_type* in) { return Decode32(in); }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return Encode32(value, out, offset);
  }
  inline static integral_t integral(integral_t value) { return value; }
  static bytes_view min() { return Minu32(); }
  static bytes_view max() { return Maxu32(); }
  static bytes_view raw_ref(const integral_t& value) {
    return bytes_view(reinterpret_cast<const byte_type*>(&value),
                      sizeof(value));
  }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
};

template<>
struct numeric_traits<int64_t> {
  typedef int64_t integral_t;
  static bytes_view min() { return Mini64(); }
  static bytes_view max() { return Maxi64(); }
  inline static integral_t integral(integral_t value) { return value; }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return Encode64(value, out, offset);
  }
  static integral_t decode(const byte_type* in) { return Decode64(in); }
};

template<>
struct numeric_traits<uint64_t> {
  typedef uint64_t integral_t;
  static integral_t decode(const byte_type* in) { return Decode64(in); }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return Encode64(value, out, offset);
  }
  inline static integral_t integral(integral_t value) { return value; }
  static bytes_view max() { return Maxu64(); }
  static bytes_view min() { return Minu64(); }
  static bytes_view raw_ref(const integral_t& value) {
    return bytes_view(reinterpret_cast<const byte_type*>(&value),
                      sizeof(value));
  }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
};

template<>
struct numeric_traits<float> {
  typedef int32_t integral_t;
  static bytes_view ninf() { return Nfinf32(); }
  static bytes_view min() { return Minf32(); }
  static bytes_view max() { return Maxf32(); }
  static bytes_view inf() { return Finf32(); }
  static float_t floating(integral_t value) { return I32tof(value); }
  static integral_t integral(float_t value) { return Ftoi32(value); }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return Encodef32(value, out, offset);
  }
  static float_t decode(const byte_type* in) { return floating(Decodef32(in)); }
};

template<>
struct numeric_traits<double> {
  typedef int64_t integral_t;
  static bytes_view ninf() { return Ndinf64(); }
  static bytes_view min() { return Mind64(); }
  static bytes_view max() { return Maxd64(); }
  static bytes_view inf() { return Dinf64(); }
  static double_t floating(integral_t value) { return I64tod(value); }
  static integral_t integral(double_t value) { return Dtoi64(value); }
  constexpr static size_t size() { return sizeof(integral_t) + 1; }
  static size_t encode(integral_t value, byte_type* out, size_t offset = 0) {
    return Encoded64(value, out, offset);
  }
  static double_t decode(const byte_type* in) {
    return floating(Decoded64(in));
  }
};

template<>
struct numeric_traits<long double> {};  // numeric_traits

}  // namespace irs::numeric_utils
