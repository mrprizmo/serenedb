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

#include "engine_selector_feature.h"

#include "app/app_server.h"
#include "basics/down_cast.h"
#include "general_server/state.h"
#include "rest_server/serened.h"
#ifdef SDB_CLUSTER
#include "cluster_engine/cluster_engine.h"
#include "rocksdb_engine/rocksdb_engine.h"
#else
#include "rocksdb_engine_catalog/rocksdb_engine_catalog.h"
#endif

namespace sdb {

EngineSelectorFeature::EngineSelectorFeature(Server& server)
  : SerenedFeature{server, name()} {
  setOptional(false);
}

void EngineSelectorFeature::prepare() {
#ifdef SDB_CLUSTER
  _engine = &SerenedServer::Instance().getFeature<RocksDBEngine>();
#else
  _engine = &SerenedServer::Instance().getFeature<RocksDBEngineCatalog>();
#endif

#ifdef SDB_CLUSTER
  if (ServerState::instance()->IsCoordinator()) {
    _engine->disable();

    auto& engine = server().getFeature<ClusterEngine>();
    engine.setActualEngine(_engine);
    _engine = &engine;
  }
#endif

  SDB_ASSERT(_engine);
}

void EngineSelectorFeature::unprepare() {
#ifdef SDB_CLUSTER
  if (ServerState::instance()->IsCoordinator()) {
    basics::downCast<ClusterEngine>(_engine)->setActualEngine(nullptr);
  }
#endif

  _engine = nullptr;
}

StorageEngine& EngineSelectorFeature::engine() {
  SDB_ASSERT(_engine);
  return *_engine;
}

bool EngineSelectorFeature::isRocksDB() {
  SDB_ASSERT(_engine);
  return _engine->typeName() == RocksDBEngineCatalog::kEngineName;
}

StorageEngine& GetServerEngine() {
  return SerenedServer::Instance().getFeature<EngineSelectorFeature>().engine();
}

}  // namespace sdb
