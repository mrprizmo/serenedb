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

#include "max_map_count_feature.h"

#include <algorithm>

#include "app/app_server.h"
#include "app/options/program_options.h"
#include "app/options/section.h"
#include "basics/file_utils.h"
#include "basics/number_of_cores.h"
#include "basics/process-utils.h"
#include "basics/string_utils.h"
#include "basics/system-functions.h"
#include "basics/thread.h"

using namespace sdb;
using namespace sdb::basics;
using namespace sdb::options;

MaxMapCountFeature::MaxMapCountFeature(Server& server)
  : SerenedFeature{server, name()} {
  setOptional(false);
}

uint64_t MaxMapCountFeature::actualMaxMappings() {
  uint64_t max_mappings = UINT64_MAX;

  // in case we cannot determine the number of max_map_count, we will
  // assume an effectively unlimited number of mappings
#ifdef __linux__
  // test max_map_count value in /proc/sys/vm
  try {
    std::string value = basics::file_utils::Slurp("/proc/sys/vm/max_map_count");

    max_mappings = basics::string_utils::Uint64(value);
  } catch (...) {
    // file not found or values not convertible into integers
  }
#endif

  return max_mappings;
}

uint64_t MaxMapCountFeature::minimumExpectedMaxMappings() {
#ifdef __linux__
  uint64_t expected = 65530;  // Linux kernel default

  uint64_t nproc = number_of_cores::GetValue();

  // we expect at most 8 times the number of cores as the effective number of
  // threads, and we want to allow at least 8000 mmaps per thread
  if (nproc * 8 * 8000 > expected) {
    expected = nproc * 8 * 8000;
  }

  return expected;
#else
  return 0;
#endif
}
