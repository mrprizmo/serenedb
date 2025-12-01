////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025 SereneDB GmbH, Berlin, Germany
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
/// Copyright holder is SereneDB GmbH, Berlin, Germany
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "app/app_feature.h"
#include "app/app_server.h"
#include "basics/containers/flat_hash_map.h"
#include "basics/resource_usage.h"
#include "general_server/scheduler_feature.h"
#include "metrics/fwd.h"
#include "pg/sql_error.h"
#include "pg_comm_task.h"

namespace sdb::pg {

class PostgresFeature final : public SerenedFeature {
 public:
  static constexpr std::string_view name() noexcept { return "postgres"; }

  explicit PostgresFeature(SerenedServer& server);

  void validateOptions(std::shared_ptr<options::ProgramOptions> options) final;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;

  uint64_t RegisterTask(PgSQLCommTaskBase& task);
  void UnregisterTask(uint64_t key);
  void CancelTaskPacket(uint64_t key);

  ResourceMonitor& CommTasksMemory() noexcept { return _comm_tasks_memory; }

  void ScheduleProcessing(std::weak_ptr<rest::CommTask> ptr) {
    server().getFeature<SchedulerFeature>().gScheduler->queue(
      RequestLane::ClientAql, [ptr]() {
        auto task = ptr.lock();
        if (task) {
          basics::downCast<PgSQLCommTaskBase>(*task).Process();
        }
      });
  }

  void ScheduleContinueProcessing(std::weak_ptr<rest::CommTask> ptr) {
    server().getFeature<SchedulerFeature>().gScheduler->queue(
      RequestLane::ClientAql, [ptr]() {
        auto task = ptr.lock();
        if (task) {
          basics::downCast<PgSQLCommTaskBase>(*task).NextRootPortal();
        }
      });
  }

 private:
  absl::Mutex _comm_tasks_mutex;
  containers::FlatHashMap<uint64_t, std::weak_ptr<rest::CommTask>> _comm_tasks;
  ResourceMonitor _comm_tasks_memory;
};

}  // namespace sdb::pg
