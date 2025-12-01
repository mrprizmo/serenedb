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
#include <functional>
#include <optional>

#include "metrics/fwd.h"

namespace sdb {

struct TransactionStatistics {
  explicit TransactionStatistics(metrics::MetricsFeature&);
  TransactionStatistics(const TransactionStatistics&) = delete;
  TransactionStatistics(TransactionStatistics&&) = delete;
  TransactionStatistics& operator=(const TransactionStatistics&) = delete;
  TransactionStatistics& operator=(TransactionStatistics&&) = delete;

  void setupDocumentMetrics();

  metrics::MetricsFeature& metrics;

  metrics::Gauge<uint64_t>& rest_transactions_memory_usage;
  metrics::Gauge<uint64_t>& internal_transactions_memory_usage;

  metrics::Counter& transactions_started;
  metrics::Counter& transactions_aborted;
  metrics::Counter& transactions_committed;
  metrics::Counter& intermediate_commits;
  metrics::Counter& read_transactions;
  metrics::Counter& dirty_read_transactions;

  // total number of lock timeouts for exclusive locks
  metrics::Counter& exclusive_lock_timeouts;
  // total number of lock timeouts for write locks
  metrics::Counter& write_lock_timeouts;
  // total duration of lock acquisition (in microseconds)
  metrics::Counter& lock_time_micros;
  // histogram for lock acquisition (in seconds)
  metrics::Histogram<metrics::LogScale<double>>& lock_times;
  // Total number of times we used a fallback to sequential locking
  metrics::Counter& sequential_locks;

  struct ReadWriteMetrics {
    // Total number of write operations in storage engine (excl. sync
    // replication)
    metrics::Counter& num_writes;
    // Total number of write operations in storage engine by sync replication
    metrics::Counter& num_writes_replication;
    // Total number of truncate operations (not number of documents truncated!)
    // (excl. sync replication)
    metrics::Counter& num_truncates;
    // Total number of truncate operations (not number of documents truncated!)
    // by sync replication
    metrics::Counter& num_truncates_replication;

    /// the following metrics are conditional and only initialized if
    /// startup option `--server.export-read-write-metrics` is set
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_read_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_insert_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_replace_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_remove_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_update_sec;
    metrics::Histogram<metrics::LogScale<float>>& rocksdb_truncate_sec;
  };

  std::optional<ReadWriteMetrics> read_write_metrics;
};

struct ServerStatistics {
  ServerStatistics(const ServerStatistics&) = delete;
  ServerStatistics(ServerStatistics&&) = delete;
  ServerStatistics& operator=(const ServerStatistics&) = delete;
  ServerStatistics& operator=(ServerStatistics&&) = delete;

  void setupDocumentMetrics();

  TransactionStatistics transactions_statistics;
  const double start_time;

  double uptime() const noexcept;

  explicit ServerStatistics(metrics::MetricsFeature& metrics, double start)
    : transactions_statistics(metrics), start_time(start) {}
};

}  // namespace sdb
