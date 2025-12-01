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

#include <rocksdb/slice.h>
#include <rocksdb/slice_transform.h>

#include "basics/common.h"
#include "basics/debugging.h"

namespace sdb {

/// effectively used for the rocksdb edge index, to allow a
/// dynamically length prefix spanning the indexed _from / _to string
class RocksDBPrefixExtractor final : public rocksdb::SliceTransform {
 public:
  RocksDBPrefixExtractor() = default;
  ~RocksDBPrefixExtractor() final = default;

  const char* Name() const final { return "RocksDBPrefixExtractor"; }

  rocksdb::Slice Transform(const rocksdb::Slice& key) const final {
    // 8-byte objectID + 0..n-byte string + 1-byte '\0'
    // + 8 byte revisionID + 1-byte '\xFF' (these are for cut off)
    SDB_ASSERT(key.size() >= sizeof(char) + sizeof(uint64_t));
    if (key.data()[key.size() - 1] == '\0') {
      // unfortunately rocksdb seems to call Tranform(Transform(k))
      return key;
    }
    SDB_ASSERT(key.data()[key.size() - 1] == '\xFF');
    SDB_ASSERT(key.size() >= sizeof(char) * 2 + sizeof(uint64_t) * 2);
    size_t l = key.size() - sizeof(uint64_t) - sizeof(char);
    SDB_ASSERT(key.data()[l - 1] == '\0');
    return rocksdb::Slice(key.data(), l);
  }

  bool InDomain(const rocksdb::Slice& key) const final {
    // 8-byte objectID + n-byte string + 1-byte '\0' + ...
    SDB_ASSERT(key.size() >= sizeof(char) + sizeof(uint64_t));
    return key.data()[key.size() - 1] != '\0';
  }

  bool InRange(const rocksdb::Slice& dst) const final {
    SDB_ASSERT(dst.size() >= sizeof(char) + sizeof(uint64_t));
    return dst.data()[dst.size() - 1] != '\0';
  }

  bool SameResultWhenAppended(const rocksdb::Slice& prefix) const final {
    return prefix.data()[prefix.size() - 1] == '\0';
  }
};

}  // namespace sdb
