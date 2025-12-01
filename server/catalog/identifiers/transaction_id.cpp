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

#include "catalog/identifiers/transaction_id.h"

#include "basics/debugging.h"
#include "database/ticks.h"

namespace sdb {

uint32_t TransactionId::serverId() const {
  return ExtractServerIdFromTick(id());
}

TransactionId TransactionId::asCoordinatorTransactionId() const {
  auto result = TransactionId{(id() & ~3)};
  SDB_ASSERT(result.isCoordinatorTransactionId());
  return result;
}

TransactionId TransactionId::asLeaderTransactionId() const {
  auto result = asCoordinatorTransactionId().child();
  SDB_ASSERT(result.isLeaderTransactionId());
  return result;
}

TransactionId TransactionId::asFollowerTransactionId() const {
  auto result = asLeaderTransactionId().child();
  SDB_ASSERT(result.isFollowerTransactionId());
  return result;
}

TransactionId TransactionId::createSingle() {
  return TransactionId(NewTickServer());
}

TransactionId TransactionId::createCoordinator() {
  return TransactionId(NewServerSpecificTickMod4());
}

TransactionId TransactionId::createDBServer() {
  return TransactionId(NewServerSpecificTickMod4() + 3);
}

}  // namespace sdb
