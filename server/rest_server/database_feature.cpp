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

#include "database_feature.h"

#include <absl/strings/str_cat.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "app/app_server.h"
#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "basics/application-exit.h"
#include "basics/errors.h"
#include "basics/exceptions.h"
#include "basics/static_strings.h"
#include "catalog/catalog.h"
#include "general_server/scheduler_feature.h"
#include "general_server/state.h"
#include "rest_server/database_feature.h"
#include "storage_engine/engine_selector_feature.h"
#include "storage_engine/storage_engine.h"
#include "storage_engine/table_shard.h"
#include "utils/query_cache.h"

#ifdef SDB_CLUSTER
#include "replication/database_replication_applier.h"
#include "replication/replication_clients.h"
#include "replication/replication_feature.h"
#endif

using namespace sdb::options;

namespace sdb {

DatabaseFeature::DatabaseFeature(Server& server)
  : SerenedFeature{server, name()} {
  setOptional(false);
}

void DatabaseFeature::validateOptions(
  std::shared_ptr<options::ProgramOptions> options) {
  // check the misuse of startup options
  if (_check_version && _upgrade) {
    SDB_FATAL("xxxxx", Logger::FIXME,
              "cannot specify both '--database.check-version' and "
              "'--database.auto-upgrade'");
  }
}

DatabaseFeature::~DatabaseFeature() = default;

void DatabaseFeature::start() {
  auto r = server().getFeature<catalog::CatalogFeature>().Open();
  if (!r.ok()) {
    SDB_THROW(std::move(r));
  }

  _started.store(true);
}

void DatabaseFeature::stop() {
  stopAppliers();

  // turn off query cache and flush it
  aql::QueryCacheProperties p{
    .mode = aql::QueryCacheMode::CacheAlwaysOff,
    .max_results_count = 0,
    .max_results_size = 0,
    .max_entry_size = 0,
    .show_bind_vars = false,
  };
  aql::QueryCache::instance()->properties(p);
  aql::QueryCache::instance()->invalidate();

  server()
    .getFeature<EngineSelectorFeature>()
    .engine()
    .cleanupReplicationContexts();
}

void DatabaseFeature::unprepare() {
  SerenedServer::Instance().getFeature<catalog::CatalogFeature>().Cleanup();
}

void DatabaseFeature::prepare() {
#ifdef SDB_CLUSTER
  if (server().hasFeature<ReplicationFeature>()) {
    _replication_feature = &server().getFeature<ReplicationFeature>();
  }
#endif
}

bool DatabaseFeature::started() const noexcept {
  return _started.load(std::memory_order_relaxed);
}

void DatabaseFeature::stopAppliers() {
  // stop the replication appliers so all replication transactions can end
  if (_replication_feature == nullptr) {
    return;
  }

#ifdef SDB_CLUSTER
  if (!ServerState::instance()->IsCoordinator()) {
    for (auto [_, applier] : GetAllReplicationAppliers()) {
      _replication_feature->stopApplier(applier);
    }
  }
#endif
}

}  // namespace sdb
