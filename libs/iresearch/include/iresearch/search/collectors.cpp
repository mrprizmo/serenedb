////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "collectors.hpp"

namespace {

using namespace irs;

struct NoopFieldCollector final : FieldCollector {
  void collect(const SubReader&, const TermReader&) final {}
  void reset() final {}
  void collect(bytes_view) final {}
  void write(DataOutput&) const final {}
};

struct NoopTermCollector final : TermCollector {
  void collect(const SubReader&, const TermReader&,
               const AttributeProvider&) final {}
  void reset() final {}
  void collect(bytes_view) final {}
  void write(DataOutput&) const final {}
};

static NoopFieldCollector gNoopFieldStats;
static NoopTermCollector gNoopTermStats;

}  // namespace

namespace irs {

FieldCollectorWrapper::collector_type& FieldCollectorWrapper::noop() noexcept {
  return gNoopFieldStats;
}

FieldCollectors::FieldCollectors(const Scorers& order)
  : CollectorsBase<FieldCollectorWrapper>(order.buckets().size(), order) {
  auto collectors = _collectors.begin();
  for (auto& bucket : order.buckets()) {
    *collectors = bucket.bucket->PrepareFieldCollector();
    SDB_ASSERT(*collectors);  // ensured by wrapper
    ++collectors;
  }
  SDB_ASSERT(collectors == _collectors.end());
}

void FieldCollectors::collect(const SubReader& segment,
                              const TermReader& field) const {
  switch (_collectors.size()) {
    case 0:
      return;
    case 1:
      _collectors.front()->collect(segment, field);
      return;
    case 2:
      _collectors.front()->collect(segment, field);
      _collectors.back()->collect(segment, field);
      return;
    default:
      for (auto& collector : _collectors) {
        collector->collect(segment, field);
      }
  }
}

void FieldCollectors::finish(byte_type* stats_buf) const {
  // special case where term statistics collection is not applicable
  // e.g. by_column_existence filter
  SDB_ASSERT(_buckets.size() == _collectors.size());

  for (size_t i = 0, count = _collectors.size(); i < count; ++i) {
    auto& sort = _buckets[i];
    SDB_ASSERT(sort.bucket);  // ensured by order::prepare

    sort.bucket->collect(
      stats_buf + sort.stats_offset,  // where stats for bucket start
      _collectors[i].get(), nullptr);
  }
}

TermCollectorWrapper::collector_type& TermCollectorWrapper::noop() noexcept {
  return gNoopTermStats;
}

TermCollectors::TermCollectors(const Scorers& buckets, size_t size)
  : CollectorsBase<TermCollectorWrapper>(buckets.buckets().size() * size,
                                         buckets) {
  // add term collectors from each bucket
  // layout order [t0.b0, t0.b1, ... t0.bN, t1.b0, t1.b1 ... tM.BN]
  auto begin = _collectors.begin();
  auto end = _collectors.end();
  for (; begin != end;) {
    for (auto& entry : buckets.buckets()) {
      SDB_ASSERT(entry.bucket);  // ensured by order::prepare

      *begin = entry.bucket->PrepareTermCollector();
      SDB_ASSERT(*begin);  // ensured by wrapper
      ++begin;
    }
  }
  SDB_ASSERT(begin == _collectors.end());
}

void TermCollectors::collect(const SubReader& segment, const TermReader& field,
                             size_t term_idx,
                             const AttributeProvider& attrs) const {
  const size_t count = _buckets.size();

  switch (count) {
    case 0:
      return;
    case 1: {
      SDB_ASSERT(term_idx < _collectors.size());
      SDB_ASSERT(_collectors[term_idx]);  // enforced by wrapper
      _collectors[term_idx]->collect(segment, field, attrs);
      return;
    }
    case 2: {
      SDB_ASSERT(term_idx + 1 < _collectors.size());
      SDB_ASSERT(_collectors[term_idx]);      // enforced by wrapper
      SDB_ASSERT(_collectors[term_idx + 1]);  // enforced by wrapper
      _collectors[term_idx]->collect(segment, field, attrs);
      _collectors[term_idx + 1]->collect(segment, field, attrs);
      return;
    }
    default: {
      const size_t term_offset_count = term_idx * count;
      for (size_t i = 0; i < count; ++i) {
        const auto idx = term_offset_count + i;
        SDB_ASSERT(
          idx <
          _collectors.size());  // enforced by allocation in the constructor
        SDB_ASSERT(_collectors[idx]);  // enforced by wrapper

        _collectors[idx]->collect(segment, field, attrs);
      }
      return;
    }
  }
}

size_t TermCollectors::push_back() {
  const size_t size = _buckets.size();
  SDB_ASSERT(0 == size || 0 == _collectors.size() % size);

  switch (size) {
    case 0:
      return 0;
    case 1: {
      const auto term_offset = _collectors.size();
      SDB_ASSERT(_buckets.front().bucket);  // ensured by order::prepare
      _collectors.emplace_back(_buckets.front().bucket->PrepareTermCollector());
      return term_offset;
    }
    case 2: {
      const auto term_offset = _collectors.size() / 2;
      SDB_ASSERT(_buckets.front().bucket);  // ensured by order::prepare
      _collectors.emplace_back(_buckets.front().bucket->PrepareTermCollector());
      SDB_ASSERT(_buckets.back().bucket);  // ensured by order::prepare
      _collectors.emplace_back(_buckets.back().bucket->PrepareTermCollector());
      return term_offset;
    }
    default: {
      const auto term_offset = _collectors.size() / size;
      _collectors.reserve(_collectors.size() + size);
      for (auto& entry : _buckets) {
        SDB_ASSERT(entry.bucket);  // ensured by order::prepare
        _collectors.emplace_back(entry.bucket->PrepareTermCollector());
      }
      return term_offset;
    }
  }
}

void TermCollectors::finish(byte_type* stats_buf, size_t term_idx,
                            const FieldCollectors& field_collectors,
                            const IndexReader& /*index*/) const {
  const auto bucket_count = _buckets.size();

  switch (bucket_count) {
    case 0:
      break;
    case 1: {
      SDB_ASSERT(field_collectors.front());
      SDB_ASSERT(_buckets.front().bucket);
      _buckets.front().bucket->collect(
        stats_buf + _buckets.front().stats_offset, field_collectors.front(),
        _collectors[term_idx].get());
    } break;
    case 2: {
      term_idx *= bucket_count;

      SDB_ASSERT(field_collectors.front());
      SDB_ASSERT(_buckets.front().bucket);
      _buckets.front().bucket->collect(
        stats_buf + _buckets.front().stats_offset, field_collectors.front(),
        _collectors[term_idx].get());

      SDB_ASSERT(field_collectors.back());
      SDB_ASSERT(_buckets.back().bucket);
      _buckets.back().bucket->collect(stats_buf + _buckets.back().stats_offset,
                                      field_collectors.back(),
                                      _collectors[term_idx + 1].get());
    } break;
    default: {
      term_idx *= bucket_count;

      auto begin = field_collectors.begin();
      for (auto& bucket : _buckets) {
        bucket.bucket->collect(stats_buf + bucket.stats_offset, begin->get(),
                               _collectors[term_idx++].get());
        ++begin;
      }
    } break;
  }
}

}  // namespace irs
