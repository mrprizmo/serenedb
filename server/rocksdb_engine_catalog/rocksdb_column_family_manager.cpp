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

#include "rocksdb_column_family_manager.h"

#include "basics/debugging.h"

namespace sdb {

std::array<const char*,
           sdb::RocksDBColumnFamilyManager::kNumberOfColumnFamilies>
  RocksDBColumnFamilyManager::gInternalNames = {"default",      "Documents",
                                                "PrimaryIndex", "EdgeIndex",
                                                "VPackIndex",   "Postgres"};

std::array<const char*,
           sdb::RocksDBColumnFamilyManager::kNumberOfColumnFamilies>
  RocksDBColumnFamilyManager::gExternalNames = {
    "definitions", "documents", "primary", "edge", "vpack", "postgres"};

std::array<rocksdb::ColumnFamilyHandle*,
           RocksDBColumnFamilyManager::kNumberOfColumnFamilies>
  RocksDBColumnFamilyManager::gHandles = {nullptr, nullptr, nullptr,
                                          nullptr, nullptr, nullptr};

rocksdb::ColumnFamilyHandle* RocksDBColumnFamilyManager::gDefaultHandle =
  nullptr;

void RocksDBColumnFamilyManager::initialize() {
  size_t index = std::to_underlying(Family::Definitions);
  gInternalNames[index] = rocksdb::kDefaultColumnFamilyName.c_str();
}

rocksdb::ColumnFamilyHandle* RocksDBColumnFamilyManager::get(Family family) {
  if (family == Family::Invalid) {
    return gDefaultHandle;
  }

  size_t index = std::to_underlying(family);
  SDB_ASSERT(index < gHandles.size());

  return gHandles[index];
}

void RocksDBColumnFamilyManager::set(Family family,
                                     rocksdb::ColumnFamilyHandle* handle) {
  if (family == Family::Invalid) {
    gDefaultHandle = handle;
    return;
  }

  size_t index = std::to_underlying(family);
  SDB_ASSERT(index < gHandles.size());

  gHandles[index] = handle;
}

const char* RocksDBColumnFamilyManager::name(Family family, NameMode mode) {
  if (family == Family::Invalid) {
    return rocksdb::kDefaultColumnFamilyName.c_str();
  }

  size_t index = std::to_underlying(family);
  SDB_ASSERT(index < gInternalNames.size());

  if (mode == NameMode::Internal) {
    return gInternalNames[index];
  }
  return gExternalNames[index];
}

const char* RocksDBColumnFamilyManager::name(
  rocksdb::ColumnFamilyHandle* handle, NameMode mode) {
  for (size_t i = 0; i < gHandles.size(); ++i) {
    if (gHandles[i] == handle) {
      if (mode == NameMode::Internal) {
        return gInternalNames[i];
      }
      return gExternalNames[i];
    }
  }

  // didn't find it in the list; we should never get here
  SDB_ASSERT(false);
  return "unknown";
}

const std::array<rocksdb::ColumnFamilyHandle*,
                 RocksDBColumnFamilyManager::kNumberOfColumnFamilies>&
RocksDBColumnFamilyManager::allHandles() {
  return gHandles;
}

}  // namespace sdb
