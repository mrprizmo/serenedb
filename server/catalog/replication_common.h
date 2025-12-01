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

#include <cstdlib>
#include <string>

#include "basics/common.h"

namespace sdb {

// TODO(mbkkt) new ids
enum ReplicationOperation : uint16_t {
  kReplicationInvalid = 0,

  kReplicationDatabaseCreate = 1100,
  kReplicationDatabaseDrop = 1101,

  kReplicationTableCreate = 2000,
  kReplicationTableDrop = 2001,
  kReplicationTableRename = 2002,
  kReplicationTableChange = 2003,
  kReplicationTableTruncate = 2004,

  kReplicationIndexCreate = 2100,
  kReplicationIndexDrop = 2101,

  kReplicationViewCreate = 2110,
  kReplicationViewDrop = 2111,
  kReplicationViewChange = 2112,

  kReplicationTransactionStart = 2200,
  kReplicationTransactionCommit = 2201,
  kReplicationTransactionAbort = 2202,

  kReplicationMarkerDocument = 2300,
  kReplicationMarkerRemove = 2302,

  kReplicationFunctionCreate = 2400,
  kReplicationFunctionDrop = 2401,

  kReplicationRoleCreate = 2500,
  kReplicationRoleDrop = 2501,
  kReplicationRoleChange = 2502,
};

// generate a timestamp string in a target buffer
void GetTimeStampReplication(char*, size_t);
void GetTimeStampReplication(double, char*, size_t);

}  // namespace sdb
