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

#include <absl/base/internal/endian.h>

#include <bit>

namespace sdb::basics {

constexpr bool IsLittleEndian() {
  return std::endian::native == std::endian::little;
}

template<typename T>
inline T HostToLittle(T in) {
  return absl::little_endian::FromHost(in);
}

template<typename T>
inline T LittleToHost(T in) {
  return absl::little_endian::ToHost(in);
}

template<typename T>
inline T HostToBig(T in) {
  return absl::big_endian::FromHost(in);
}

template<typename T>
inline T BigToHost(T in) {
  return absl::big_endian::ToHost(in);
}

}  // namespace sdb::basics
