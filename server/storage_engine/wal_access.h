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

#include <map>

#include "basics/containers/flat_hash_set.h"
#include "basics/result.h"
#include "catalog/fwd.h"
#include "catalog/identifiers/object_id.h"
#include "catalog/identifiers/transaction_id.h"
#include "catalog/types.h"
#include "rest_server/serened.h"

namespace sdb {

struct WalAccessResult {
  WalAccessResult() : WalAccessResult(ERROR_OK, false, 0, 0, 0) {}

  WalAccessResult(ErrorCode code, bool ft, Tick included,
                  Tick last_scanned_tick, Tick latest)
    : _result(code),
      _from_tick_included(ft),
      _last_included_tick(included),
      _last_scanned_tick(last_scanned_tick),
      _latest_tick(latest) {}

  bool fromTickIncluded() const { return _from_tick_included; }
  Tick lastIncludedTick() const { return _last_included_tick; }
  Tick lastScannedTick() const { return _last_scanned_tick; }
  void lastScannedTick(Tick tick) { _last_scanned_tick = tick; }
  Tick latestTick() const { return _latest_tick; }

  WalAccessResult& reset(ErrorCode error_number, bool ft, Tick included,
                         Tick last_scanned_tick, Tick latest) {
    _result.reset(error_number);
    _from_tick_included = ft;
    _last_included_tick = included;
    _last_scanned_tick = last_scanned_tick;
    _latest_tick = latest;
    return *this;
  }

  // forwarded methods
  bool ok() const { return _result.ok(); }
  bool fail() const { return _result.fail(); }
  ErrorCode errorNumber() const { return _result.errorNumber(); }
  std::string_view errorMessage() const { return _result.errorMessage(); }
  void reset(const Result& other) { _result.reset(); }

  // access methods
  const Result& result() const& { return _result; }
  Result result() && { return std::move(_result); }

 private:
  Result _result;
  bool _from_tick_included;
  Tick _last_included_tick;
  Tick _last_scanned_tick;
  Tick _latest_tick;
};

/// StorageEngine agnostic wal access interface.
/// TODO: add methods for _admin/wal/ and get rid of engine specific handlers
class WalAccess {
  WalAccess(const WalAccess&) = delete;
  WalAccess& operator=(const WalAccess&) = delete;

 protected:
  WalAccess() = default;
  virtual ~WalAccess() = default;

 public:
  struct Filter {
    Filter() = default;

    /// tick last scanned by the last iteration
    /// is used to find batches in rocksdb
    uint64_t tick_last_scanned = 0;

    /// first tick to use
    uint64_t tick_start = 0;

    /// last tick to include
    uint64_t tick_end = UINT64_MAX;

    /// only output markers from this database
    ObjectId database;
    /// Only output data from this collection
    /// FIXME: make a set of collections
    ObjectId collection = ObjectId::none();

    /// only include these transactions, up to
    /// (not including) firstRegularTick
    containers::FlatHashSet<TransactionId> transaction_ids;
    /// starting from this tick ignore transactionIds
    Tick first_regular_tick = 0;
  };

  typedef std::function<void(vpack::Slice)> MarkerCallback;
  typedef std::function<void(TransactionId, TransactionId)> TransactionCallback;

  /// {"tickMin":"123", "tickMax":"456",
  ///  "server":{"version":"3.2", "serverId":"abc"}}
  virtual Result tickRange(std::pair<Tick, Tick>& min_max) const = 0;

  /// {"lastTick":"123",
  ///  "server":{"version":"3.2",
  ///  "serverId":"abc"},
  ///  "clients": {
  ///    "serverId": "ass", "lastTick":"123", ...
  ///  }}
  ///
  virtual Tick lastTick() const = 0;

  virtual WalAccessResult tail(const Filter& filter, size_t chunk_size,
                               const MarkerCallback&) const = 0;
};

/// helper class used to resolve databases
///        and collections from wal markers in an efficient way
struct WalAccessContext {
  WalAccessContext(const WalAccess::Filter& filter,
                   const WalAccess::MarkerCallback& c)
    : filter(filter), callback(c), response_size(0) {}

  ~WalAccessContext() = default;

  /// check if db should be handled, might already be deleted
  bool shouldHandleDB(ObjectId dbid) const;

  bool shouldHandleFunction(ObjectId dbid, ObjectId vid) const;

  /// check if view should be handled, might already be deleted
  bool shouldHandleView(ObjectId dbid, ObjectId vid) const;

  /// Check if collection is in filter, will load collection
  /// and prevent deletion
  bool shouldHandleCollection(ObjectId dbid, ObjectId cid);

  /// try to get collection, may return null
  const catalog::Database* LoadDatabase(ObjectId dbid);

  catalog::Table* loadCollection(ObjectId dbid, ObjectId cid);

 public:
  /// arbitrary collection filter (inclusive)
  const WalAccess::Filter filter;
  WalAccess::MarkerCallback callback;

  size_t response_size;

  vpack::Builder builder;

  std::map<ObjectId, std::shared_ptr<catalog::Database>> databases;

  std::map<ObjectId, std::shared_ptr<catalog::Table>> collections;
};

}  // namespace sdb
