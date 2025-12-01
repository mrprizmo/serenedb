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

#include <vpack/slice.h>

#include <magic_enum/magic_enum.hpp>
#include <memory>
#include <mutex>
#include <vector>

#include "basics/common.h"
#include "basics/containers/flat_hash_map.h"
#include "basics/containers/flat_hash_set.h"
#include "basics/containers/node_hash_map.h"
#include "basics/read_write_lock.h"
#include "catalog/identifiers/object_id.h"
#include "catalog/types.h"
#include "utils/query_string.h"

namespace vpack {

class Builder;

}  // namespace vpack
namespace sdb {
namespace aql {

enum class QueryCacheMode {
  CacheAlwaysOff,
  CacheAlwaysOn,
  CacheOnDemand,
};

struct QueryCacheProperties {
  QueryCacheMode mode;
  uint64_t max_results_count;
  uint64_t max_results_size;
  uint64_t max_entry_size;
  bool show_bind_vars;
};

struct QueryCacheResultEntry {
  QueryCacheResultEntry() = delete;

  QueryCacheResultEntry(
    uint64_t hash, const QueryString& query_string,
    const std::shared_ptr<vpack::Builder>& query_result,
    const std::shared_ptr<vpack::Builder>& bind_vars,
    containers::FlatHashMap<ObjectId, std::string>&& data_sources);

  const uint64_t hash;
  const std::string query_string;
  const std::shared_ptr<vpack::Builder> query_result;
  const std::shared_ptr<vpack::Builder> bind_vars;
  const containers::FlatHashMap<ObjectId, std::string> data_sources;
  std::shared_ptr<vpack::Builder> stats;
  size_t size;
  size_t rows{0};
  std::atomic<uint64_t> hits{0};
  double stamp{0.};
  QueryCacheResultEntry* prev{};
  QueryCacheResultEntry* next{};

  void increaseHits() { hits.fetch_add(1, std::memory_order_relaxed); }
  double executionTime() const;

  void toVPack(vpack::Builder& builder) const;
};

struct QueryCacheDatabaseEntry {
  QueryCacheDatabaseEntry(const QueryCacheDatabaseEntry&) = delete;
  QueryCacheDatabaseEntry& operator=(const QueryCacheDatabaseEntry&) = delete;

  /// create a database-specific cache
  QueryCacheDatabaseEntry();

  /// destroy a database-specific cache
  ~QueryCacheDatabaseEntry();

  /// lookup a query result in the database-specific cache
  std::shared_ptr<QueryCacheResultEntry> lookup(
    uint64_t hash, const QueryString& query_string,
    const std::shared_ptr<vpack::Builder>& bind_vars) const;

  /// store a query result in the database-specific cache
  void store(std::shared_ptr<QueryCacheResultEntry>&& entry,
             size_t allowed_max_results_count, size_t allowed_max_results_size);

  template<typename C>
  void invalidate(const C& data_source_ids) {
    for (auto& it : data_source_ids) {
      invalidate(it);
    }
  }

  /// invalidate all entries for a data source in the
  /// database-specific cache
  void invalidate(ObjectId object_id);

  void queriesToVPack(vpack::Builder& builder) const;

  /// enforce maximum number of results
  /// must be called under the shard's lock
  void enforceMaxResults(size_t num_results, size_t size_results);

  /// enforce maximum size of individual entries
  /// must be called under the shard's lock
  void enforceMaxEntrySize(size_t value);

  /// exclude all data from system collections
  /// must be called under the shard's lock
  void excludeSystem();

  /// unlink the result entry from all datasource maps
  void removeDatasources(const QueryCacheResultEntry* e);

  /// unlink the result entry from the list
  void unlink(QueryCacheResultEntry*);

  /// link the result entry to the end of the list
  void link(QueryCacheResultEntry*);

  /// hash table that maps query hashes to query results
  containers::FlatHashMap<uint64_t, std::shared_ptr<QueryCacheResultEntry>>
    entries_by_hash;

  /// hash table that contains all data souce-specific query results
  ///        maps from data sources GUIDs to a set of query results as defined
  ///        in
  /// _entries_by_hash
  containers::NodeHashMap<ObjectId,
                          std::pair<bool, containers::FlatHashSet<uint64_t>>>
    entries_by_data_source_guid;

  /// beginning of linked list of result entries
  QueryCacheResultEntry* head{};

  /// end of linked list of result entries
  QueryCacheResultEntry* tail{};

  /// number of results in this cache
  size_t num_results{0};

  /// total size of results in this cache
  size_t size_results{0};
};

class QueryCache {
 public:
  QueryCache(const QueryCache&) = delete;
  QueryCache& operator=(const QueryCache&) = delete;
  QueryCache() = default;

  ~QueryCache();

  QueryCacheProperties properties() const;

  void properties(const QueryCacheProperties& properties);
  void properties(vpack::Slice properties);
  void toVPack(vpack::Builder& builder) const;

  /// test whether the cache might be active
  /// this is a quick test that may save the caller from further bothering
  /// about the query cache if case it returns `false`
  bool mayBeActive() const;

  QueryCacheMode mode() const;

  /// lookup a query result in the cache
  std::shared_ptr<QueryCacheResultEntry> lookup(
    ObjectId database_id, uint64_t hash, const QueryString& query_string,
    const std::shared_ptr<vpack::Builder>& bind_vars) const;

  /// store a query cache entry in the cache
  void store(ObjectId database_id,
             std::shared_ptr<QueryCacheResultEntry> entry);

  /// invalidate all queries for the given data sources
  void invalidate(ObjectId database_id,
                  std::span<const ObjectId> data_source_ids);

  /// invalidate all queries for a particular data source
  void invalidate(ObjectId database_id, ObjectId object_id);

  /// invalidate all queries for a particular database
  void invalidate(ObjectId database_id);

  /// invalidate all queries
  void invalidate();

  /// get the pointer to the global query cache
  static QueryCache* instance();

  /// create a vpack representation of the queries in the cache
  void queriesToVPack(ObjectId database_id, vpack::Builder& builder) const;

 private:
  /// invalidate all entries in the cache part
  /// note that the caller of this method must hold the write lock
  void invalidate(unsigned int part);

  /// enforce maximum number of results in each database-specific cache
  /// must be called under the cache's properties lock
  void enforceMaxResults(size_t num_results, size_t size_results);

  /// enforce maximum size of individual entries in each
  /// database-specific cache must be called under the cache's properties lock
  void enforceMaxEntrySize(size_t value);

  /// exclude all data from system collections
  void excludeSystem();

  /// sets the maximum number of elements in the cache
  void setMaxResults(size_t num_results, size_t size_results);

  /// sets the maximum size for each entry in the cache
  void setMaxEntrySize(size_t value);

  /// enable or disable the query cache
  void setMode(QueryCacheMode);

  /// determine which part of the cache to use for the cache entries
  unsigned int getPart(ObjectId database_id) const;

 private:
  /// number of R/W locks for the query cache
  static constexpr uint64_t kNumberOfParts = 16;

  /// protect mode changes with a mutex
  mutable absl::Mutex _properties_lock;

  /// read-write lock for the cache
  mutable absl::Mutex _entries_lock[kNumberOfParts];

  /// cached query entries, organized per database
  containers::FlatHashMap<ObjectId, std::unique_ptr<QueryCacheDatabaseEntry>>
    _entries[kNumberOfParts];
};

}  // namespace aql
}  // namespace sdb

namespace magic_enum {

template<>
[[maybe_unused]] constexpr customize::customize_t
customize::enum_name<sdb::aql::QueryCacheMode>(
  sdb::aql::QueryCacheMode value) noexcept {
  switch (value) {
    case sdb::aql::QueryCacheMode::CacheAlwaysOff:
      return "off";
    case sdb::aql::QueryCacheMode::CacheAlwaysOn:
      return "on";
    case sdb::aql::QueryCacheMode::CacheOnDemand:
      return "demand";
  }
  return invalid_tag;
}

}  // namespace magic_enum
