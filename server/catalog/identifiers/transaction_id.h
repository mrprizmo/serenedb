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

#include "basics/identifier.h"
#include "database/ticks.h"

namespace sdb {

class TransactionId final : public basics::Identifier {
 public:
  using Identifier::Identifier;

  static constexpr TransactionId none() { return TransactionId{0}; }

  static TransactionId createSingle();

  static TransactionId createCoordinator();
  [[deprecated]] static TransactionId createDBServer();

  bool isSet() const { return id() != 0; }

  bool isCoordinatorTransactionId() const { return id() % 4 == 0; }
  [[deprecated]] bool isDBServerTransactionId() const { return id() % 4 == 3; }

  [[deprecated]] bool isLeaderTransactionId() const { return id() % 4 == 1; }
  [[deprecated]] bool isFollowerTransactionId() const { return id() % 4 == 2; }

  [[deprecated]] bool isChildTransactionId() const {
    return isLeaderTransactionId() || isFollowerTransactionId();
  }

  uint32_t serverId() const;

  /// create a child transaction (coordinator -> dbserver; leader -> follower)
  TransactionId child() const { return TransactionId{id() + 1}; }

  TransactionId asCoordinatorTransactionId() const;
  TransactionId asLeaderTransactionId() const;
  TransactionId asFollowerTransactionId() const;
};

static_assert(sizeof(TransactionId) == sizeof(TransactionId::BaseType));

}  // namespace sdb
