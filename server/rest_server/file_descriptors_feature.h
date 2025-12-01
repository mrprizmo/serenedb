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

#include <absl/synchronization/mutex.h>

#include <chrono>
#include <mutex>

#include "app/app_feature.h"
#include "basics/operating-system.h"
#include "metrics/fwd.h"
#include "rest_server/serened.h"

#ifdef SERENEDB_HAVE_GETRLIMIT
namespace sdb {

class FileDescriptorsFeature : public SerenedFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "FileDescriptors";
  }

  explicit FileDescriptorsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;
  void prepare() final;

  // count the number of open files, by scanning /proc/self/fd.
  // note: this can be expensive
  void countOpenFiles();

  // same as countOpenFiles(), but prevents multiple threads from counting
  // at the same time, and only recounts if at a last 30 seconds have
  // passed since the last counting
  void countOpenFilesIfNeeded();

 private:
  uint64_t _count_descriptors_interval;

  metrics::Gauge<uint64_t>& _file_descriptors_current;
  metrics::Gauge<uint64_t>& _file_descriptors_limit;

  // mutex for counting open file in countOpenFilesIfNeeds.
  // this mutex prevents multiple callers from entering the function at
  // the same time, causing excessive IO for directory iteration.
  // additionally, it protects _last_count_stamp, so that only one thread
  // at a time wil do the counting and check/update _last_count_stamp.
  // this also prevents overly eager re-counting in case we have counted
  // only recented.
  absl::Mutex _last_count_mutex;
  std::chrono::steady_clock::time_point _last_count_stamp;
};

}  // namespace sdb
#endif
