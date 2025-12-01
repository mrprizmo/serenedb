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

#include <cstdint>
#include <iosfwd>
#include <string>

namespace sdb::transaction {

class Hints {
 public:
  using ValueType = uint32_t;

  /// individual hint flags that can be used for transactions.
  /// note: these values are not persisted anywhere and should not be
  /// persistend anywhere, at least not in numeric form.
  enum class Hint : ValueType {
    NONE = 0,
    SingleOperation = 1,
    LockNever = 2,
    NoIndexing = 4,           // use DisableIndexing for RocksDB
    IntermediateCommits = 8,  // enable intermediate commits in rocksdb
    AllowRangeDelete = 16,    // enable range-delete in rocksdb
    FromToplevelAql = 32,     // transaction is only runnning one AQL query
    GlobalManaged = 64,       // transaction with externally managed lifetime
    IndexCreation = 128,      // transaction is for creating index on existing
                          // collection (many inserts, no removes, index will
                          // be deleted on any failure anyway)
    IsFollowerTrx =
      256,  // transaction used to replicate something on a follower
    AllowFastLockRoundCluster =
      512  // allow the coordinator to try a fast-lock path (parallel on all
           // DBServers), and if that fails revert to slow-lock path
  };

  Hints() noexcept : _value(0) {}
  explicit Hints(Hint value) noexcept : _value(static_cast<ValueType>(value)) {}
  explicit Hints(ValueType value) noexcept : _value(value) {}

  bool has(ValueType value) const noexcept { return (_value & value) != 0; }

  bool has(Hint value) const noexcept {
    return has(static_cast<ValueType>(value));
  }

  void set(ValueType value) noexcept { _value |= value; }

  void set(Hint value) noexcept { set(static_cast<ValueType>(value)); }

  void unset(ValueType value) noexcept { _value &= ~value; }

  void unset(Hint value) noexcept { unset(static_cast<ValueType>(value)); }

  ValueType toInt() const noexcept { return static_cast<ValueType>(_value); }

  std::string toString() const;

 private:
  ValueType _value;
};

}  // namespace sdb::transaction
