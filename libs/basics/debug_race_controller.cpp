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

#ifdef SDB_DEV

#include "debug_race_controller.h"

#include <absl/cleanup/cleanup.h>

#include "app/app_server.h"

namespace sdb::basics {

static DebugRaceController gInstance{};

DebugRaceController& DebugRaceController::sharedInstance() { return gInstance; }

void DebugRaceController::reset() {
  absl::MutexLock guard{&_mutex};
  _data.clear();
  _cond_variable.notify_all();
}

bool DebugRaceController::didTrigger(
  size_t number_of_threads_to_wait_for) const {
  SDB_ASSERT(_data.size() <= number_of_threads_to_wait_for);
  return number_of_threads_to_wait_for == _data.size();
}

auto DebugRaceController::waitForOthers(size_t number_of_threads_to_wait_for,
                                        std::any my_data,
                                        const app::AppServer& server)
  -> std::optional<std::vector<std::any>> {
  absl::Cleanup notify_guard = [&] noexcept { _cond_variable.notify_all(); };
  {
    absl::MutexLock guard{&_mutex};

    if (didTrigger(number_of_threads_to_wait_for)) {
      return std::nullopt;
    }

    _data.reserve(number_of_threads_to_wait_for);
    _data.emplace_back(std::move(my_data));

    // check empty to continue after being reset
    while (!_data.empty() && _data.size() != number_of_threads_to_wait_for &&
           !server.isStopping()) {
      _cond_variable.Wait(&_mutex);
    }

    if (didTrigger(number_of_threads_to_wait_for)) {
      return {_data};
    } else {
      return std::nullopt;
    }
  }
}

}  // namespace sdb::basics

#endif
