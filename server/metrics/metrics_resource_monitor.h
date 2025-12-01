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

#include "metrics/gauge.h"
#include "metrics/metric.h"

namespace sdb::metrics {

template<typename Gauge>
class GaugeResourceMonitor {
  GaugeResourceMonitor(const GaugeResourceMonitor& other) = delete;
  GaugeResourceMonitor& operator=(const GaugeResourceMonitor& other) = delete;

 public:
  explicit GaugeResourceMonitor(Gauge& m) noexcept : _metric(m) {}

  /// increase memory usage by <value> bytes. may throw!
  void increaseMemoryUsage(uint64_t value) { _metric.fetch_add(value); }

  /// decrease memory usage by <value> bytes. will not throw
  void decreaseMemoryUsage(uint64_t value) noexcept {
    _metric.fetch_sub(value);
  }

 private:
  Gauge& _metric;
};

}  // namespace sdb::metrics
