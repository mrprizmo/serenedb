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

#include <limits>
#include <memory>
#include <string>

#include "basics/containers/flat_hash_map.h"
#include "basics/reboot_id.h"
#include "basics/resource_usage.h"
#include "basics/result.h"
#include "catalog/identifiers/shard_id.h"

namespace sdb {

using ServerID = std::string;
using DatabaseID = std::string;
using CollectionID = std::string;
using ServerShortID = uint32_t;

enum class ServerHealth {
  Good,
  Bad,
  Failed,
  Unclear,
};

struct ServerHealthState {
  RebootId reboot_id;
  ServerHealth status = ServerHealth::Unclear;
};

using ServersKnown = containers::FlatHashMap<ServerID, ServerHealthState>;
using ShardMap = containers::FlatHashMap<ShardID, std::vector<ServerID>>;

}  // namespace sdb
