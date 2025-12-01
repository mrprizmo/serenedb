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

#include "app/app_feature.h"
#include "rest_server/serened.h"

namespace sdb {

struct DumpLimits {
  // per-dump value
  uint64_t docs_per_batch_lower_bound = 10;
  // per-dump value
  uint64_t docs_per_batch_upper_bound = 1 * 1000 * 1000;
  // per-dump value
  uint64_t batch_size_lower_bound = 4 * 1024;
  // per-dump value
  uint64_t batch_size_upper_bound = 1024 * 1024 * 1024;
  // per-dump value
  uint64_t parallelism_lower_bound = 1;
  // per-dump value
  uint64_t parallelism_upper_bound = 8;
  // server-global. value will be overridden in the .cpp file.
  uint64_t memory_usage = 512 * 1024 * 1024;
};

class DumpLimitsFeature : public SerenedFeature {
 public:
  static constexpr std::string_view name() noexcept { return "DumpLimits"; }

  explicit DumpLimitsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;

  const DumpLimits& limits() const noexcept { return _dump_limits; }

 private:
  DumpLimits _dump_limits;
};

}  // namespace sdb
