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

#include <atomic>
#include <cstdint>

#include "basics/assert.h"
#include "basics/common.h"

namespace sdb {

class alignas(64) GlobalResourceMonitor final {
 public:
  constexpr GlobalResourceMonitor() = default;
  GlobalResourceMonitor(const GlobalResourceMonitor&) = delete;
  GlobalResourceMonitor& operator=(const GlobalResourceMonitor&) = delete;

  struct Stats {
    uint64_t global_limit_reached;
    uint64_t local_limit_reached;
  };

  void memoryLimit(int64_t value) noexcept {
    SDB_ASSERT(value >= 0);
    _limit = value;
  }

  int64_t memoryLimit() const noexcept { return _limit; }

  /// return the current global memory usage
  int64_t current() const noexcept;

  /// number of times the global and any local limits were reached
  Stats stats() const noexcept;

  /// increase the counter for global memory limit violations
  void trackGlobalViolation() noexcept;

  /// increase the counter for local memory limit violations
  void trackLocalViolation() noexcept;

  /// increase global memory usage by <value> bytes. if increasing
  /// exceeds the memory limit, does not perform the increase and returns false.
  /// if increasing succeeds, the global value is modified and true is returned
  /// Note: value must be >= 0
  [[nodiscard]] bool increaseMemoryUsage(int64_t value) noexcept;

  /// decrease current global memory usage by <value> bytes. will not
  /// throw Note: value must be >= 0
  void decreaseMemoryUsage(int64_t value) noexcept;

  /// Unconditionally updates the current memory usage with the given
  /// value. Since the parameter is signed, this method can increase or
  /// decrease!
  void forceUpdateMemoryUsage(int64_t value) noexcept;

  /// returns a reference to a global shared instance
  static GlobalResourceMonitor& instance() noexcept;

 private:
  /// the current combined memory usage of all tracked operations.
  /// Theoretically it can happen that the global limit is exceeded due to the
  /// correction applied as part of the rollback in increaseMemoryUsage, but at
  /// least this excess is bounded. This counter is updated by local instances
  /// only if there is a substantial allocation/deallocation. it is
  /// intentionally _not_ updated on every small allocation/deallocation. the
  /// granularity for the values in this counter is chunkSize.
  std::atomic<int64_t> _current{0};

  /// maximum allowed global memory limit for all tracked operations
  /// combined. a value of 0 means that there will be no global limit enforced.
  int64_t _limit{0};

  /// number of times the global memory limit was reached
  std::atomic<uint64_t> _global_limit_reached_counter{0};

  /// number of times a local memory limit was reached
  std::atomic<uint64_t> _local_limit_reached_counter{0};
};

}  // namespace sdb
