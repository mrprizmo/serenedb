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

#include <vpack/builder.h>
#include <vpack/slice.h>

#include <atomic>

#include "basics/debugging.h"
#include "general_server/scheduler.h"
#include "rest_server/serened.h"

namespace sdb {

class SoftShutdownTracker;

class SoftShutdownFeature final : public SerenedFeature {
 public:
  static constexpr std::string_view name() noexcept { return "SoftShutdown"; }

  explicit SoftShutdownFeature(Server& server);
  SoftShutdownTracker& softShutdownTracker() const {
    SDB_ASSERT(_soft_shutdown_tracker != nullptr);
    return *_soft_shutdown_tracker;
  }

  void beginShutdown() final;

 private:
  std::shared_ptr<SoftShutdownTracker> _soft_shutdown_tracker;
};

class SoftShutdownTracker
  : public std::enable_shared_from_this<SoftShutdownTracker> {
  // This is a class which tracks the proceedings in case of a soft shutdown.
  // Soft shutdown is a means to shut down a coordinator gracefully. It
  // means that certain things are allowed to run to completion but
  // new instances are no longer allowed to start. This class tracks
  // the number of these things in flight, so that the real shut down
  // can be triggered, once all tracked activity has ceased.
  // This class has customers, like the CursorRepositories of each vocbase,
  // these customers can on creation get a reference to an atomic counter,
  // which they can increase and decrease, the highest bit in the counter
  // is initially set, soft shutdown state is indicated when the highest
  // bit in each counter is reset. Then no new activity should be begun.

 private:
  SerenedServer& _server;
  std::atomic<bool>
    _soft_shutdown_ongoing;  // flag, if soft shutdown is ongoing
  absl::Mutex _work_item_mutex;
  Scheduler::WorkHandle _work_item;  // used for soft shutdown checker
  std::function<void(bool)> _check_func;

 public:
  struct Status {
    uint64_t aql_cursors{0};
    uint64_t transactions{0};
    uint64_t pending_jobs{0};
    uint64_t done_jobs{0};
    uint64_t low_prio_ongoing_requests{0};
    uint64_t low_prio_queued_requests{0};

    const bool soft_shutdown_ongoing;

    explicit Status(bool soft_shutdown_ongoing)
      : soft_shutdown_ongoing(soft_shutdown_ongoing) {}

    bool allClear() const noexcept {
      return aql_cursors == 0 && transactions == 0 && pending_jobs == 0 &&
             done_jobs == 0 && low_prio_ongoing_requests == 0 &&
             low_prio_queued_requests == 0;
    }
  };

  explicit SoftShutdownTracker(SerenedServer& server);
  ~SoftShutdownTracker() {}

  void initiateSoftShutdown();

  void cancelChecker();

  bool softShutdownOngoing() const {
    return _soft_shutdown_ongoing.load(std::memory_order_relaxed);
  }

  const std::atomic<bool>* getSoftShutdownFlag() const {
    return &_soft_shutdown_ongoing;
  }

  Status getStatus() const;

  void toVPack(vpack::Builder& builder) {
    Status status = getStatus();
    toVPack(builder, status);
  }

  static void toVPack(vpack::Builder& builder, const Status& status);

 private:
  bool checkAndShutdownIfAllClear() const;
  // returns true if actual shutdown triggered
  void initiateActualShutdown() const;
};

}  // namespace sdb
