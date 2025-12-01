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

#include "server_statistics.h"

#include "app/app_feature.h"
#include "metrics/counter_builder.h"
#include "metrics/gauge_builder.h"
#include "metrics/histogram_builder.h"
#include "metrics/log_scale.h"
#include "metrics/metrics_feature.h"
#include "statistics/statistics_feature.h"

using namespace sdb;

template<typename T = float>
struct TimeScale {
  static metrics::LogScale<T> scale() { return {10., 0.0, 1000.0, 11}; }
};

DECLARE_COUNTER(serenedb_collection_lock_acquisition_micros_total,
                "Total amount of collection lock acquisition time [μs]");
DECLARE_HISTOGRAM(serenedb_collection_lock_acquisition_time, TimeScale<double>,
                  "Collection lock acquisition time histogram [s]");
DECLARE_COUNTER(serenedb_collection_lock_sequential_mode_total,
                "Number of transactions using sequential locking of "
                "collections to avoid deadlocking");
DECLARE_COUNTER(
  serenedb_collection_lock_timeouts_exclusive_total,
  "Number of timeouts when trying to acquire collection exclusive locks");
DECLARE_COUNTER(
  serenedb_collection_lock_timeouts_write_total,
  "Number of timeouts when trying to acquire collection write locks");
DECLARE_GAUGE(serenedb_transactions_rest_memory_usage, uint64_t,
              "Memory usage of transactions (excl. AQL queries)");
DECLARE_GAUGE(serenedb_transactions_internal_memory_usage, uint64_t,
              "Memory usage of internal transactions");
DECLARE_COUNTER(serenedb_transactions_aborted_total,
                "Number of transactions aborted");
DECLARE_COUNTER(serenedb_transactions_committed_total,
                "Number of transactions committed");
DECLARE_COUNTER(serenedb_transactions_started_total,
                "Number of transactions started");
DECLARE_COUNTER(serenedb_intermediate_commits_total,
                "Number of intermediate commits performed in transactions");
DECLARE_COUNTER(serenedb_read_transactions_total,
                "Number of read transactions");
DECLARE_COUNTER(serenedb_dirty_read_transactions_total,
                "Number of read transactions which can do dirty reads");

DECLARE_COUNTER(serenedb_collection_truncates_total,
                "Total number of collection truncate operations (excl. "
                "synchronous replication)");
DECLARE_COUNTER(serenedb_collection_truncates_replication_total,
                "Total number of collection truncate operations by synchronous "
                "replication");
DECLARE_COUNTER(serenedb_document_writes_total,
                "Total number of document write operations (excl. synchronous "
                "replication)");
DECLARE_COUNTER(
  serenedb_document_writes_replication_total,
  "Total number of document write operations by synchronous replication");
DECLARE_HISTOGRAM(serenedb_document_read_time, TimeScale<>,
                  "Total time spent in document read operations [s]");
DECLARE_HISTOGRAM(serenedb_document_insert_time, TimeScale<>,
                  "Total time spent in document insert operations [s]");
DECLARE_HISTOGRAM(serenedb_document_replace_time, TimeScale<>,
                  "Total time spent in document replace operations [s]");
DECLARE_HISTOGRAM(serenedb_document_remove_time, TimeScale<>,
                  "Total time spent in document remove operations [s]");
DECLARE_HISTOGRAM(serenedb_document_update_time, TimeScale<>,
                  "Total time spent in document update operations [s]");
DECLARE_HISTOGRAM(serenedb_collection_truncate_time, TimeScale<>,
                  "Total time spent in collection truncate operations [s]");

TransactionStatistics::TransactionStatistics(metrics::MetricsFeature& metrics)
  : metrics(metrics),
    rest_transactions_memory_usage(
      metrics.add(serenedb_transactions_rest_memory_usage{})),
    internal_transactions_memory_usage(
      metrics.add(serenedb_transactions_internal_memory_usage{})),
    transactions_started(metrics.add(serenedb_transactions_started_total{})),
    transactions_aborted(metrics.add(serenedb_transactions_aborted_total{})),
    transactions_committed(
      metrics.add(serenedb_transactions_committed_total{})),
    intermediate_commits(metrics.add(serenedb_intermediate_commits_total{})),
    read_transactions(metrics.add(serenedb_read_transactions_total{})),
    dirty_read_transactions(
      metrics.add(serenedb_dirty_read_transactions_total{})),
    exclusive_lock_timeouts(
      metrics.add(serenedb_collection_lock_timeouts_exclusive_total{})),
    write_lock_timeouts(
      metrics.add(serenedb_collection_lock_timeouts_write_total{})),
    lock_time_micros(
      metrics.add(serenedb_collection_lock_acquisition_micros_total{})),
    lock_times(metrics.add(serenedb_collection_lock_acquisition_time{})),
    sequential_locks(
      metrics.add(serenedb_collection_lock_sequential_mode_total{})) {}

void TransactionStatistics::setupDocumentMetrics() {
  // the following metrics are conditional, so we don't initialize them in the
  // constructor
  read_write_metrics.emplace(ReadWriteMetrics{
    metrics.add(serenedb_document_writes_total{}),
    metrics.add(serenedb_document_writes_replication_total{}),
    metrics.add(serenedb_collection_truncates_total{}),
    metrics.add(serenedb_collection_truncates_replication_total{}),
    metrics.add(serenedb_document_read_time{}),
    metrics.add(serenedb_document_insert_time{}),
    metrics.add(serenedb_document_replace_time{}),
    metrics.add(serenedb_document_remove_time{}),
    metrics.add(serenedb_document_update_time{}),
    metrics.add(serenedb_collection_truncate_time{}),
  });
}

void ServerStatistics::setupDocumentMetrics() {
  transactions_statistics.setupDocumentMetrics();
}

double ServerStatistics::uptime() const noexcept {
  return StatisticsFeature::time() - start_time;
}
