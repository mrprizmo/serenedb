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

#include "basics/hashes.h"

namespace sdb {

static constexpr uint64_t kMagicPrime = 0x00000100000001b3ULL;

/// the FNV hash work horse
static inline uint64_t FnvWork(uint8_t value, uint64_t hash) noexcept {
  return (hash ^ value) * kMagicPrime;
}

/// computes a FNV hash for strings with a length
uint64_t FnvHashBlock(uint64_t hash, const void* buffer,
                      size_t length) noexcept {
  const auto* p = static_cast<const uint8_t*>(buffer);
  const auto* end = p + length;

  while (p < end) {
    hash = FnvWork(*p++, hash);
  }

  return hash;
}

uint64_t FnvHashPointer(const void* buffer, size_t length) noexcept {
  return FnvHashBlock(0xcbf29ce484222325ULL, buffer, length);
}

uint64_t FnvHashString(const char* buffer) noexcept {
  const auto* p_first = reinterpret_cast<const uint8_t*>(buffer);
  uint64_t n_hash_val = 0xcbf29ce484222325ULL;
  while (*p_first) {
    n_hash_val ^= *p_first++;
    n_hash_val *= kMagicPrime;
  }
  return n_hash_val;
}

}  // namespace sdb
