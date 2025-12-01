////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>

#include "iresearch/index/index_meta.hpp"
#include "iresearch/index/index_reader.hpp"
#include "iresearch/utils/directory_utils.hpp"
#include "iresearch/utils/hash_utils.hpp"

namespace irs {

// Reader holds file refs to files from the segment.
class SegmentReaderImpl final : public SubReader {
  struct PrivateTag final {
    explicit PrivateTag() = default;
  };

 public:
  SegmentReaderImpl(PrivateTag, const SegmentMeta& meta) noexcept
    : _info{meta}, _docs_mask{meta.docs_mask} {
    SDB_ASSERT(meta.live_docs_count <= meta.docs_count);
    SDB_ASSERT(RemovalCount(meta) <= meta.docs_count);
    // We need this logic only for splitted segments, we need to open
    // segment without removals, but they are already accounted in
    // live_docs_count.
    _info.live_docs_count = meta.docs_count - RemovalCount(meta);
  }

  static std::shared_ptr<const SegmentReaderImpl> Open(
    const Directory& dir, const SegmentMeta& meta,
    const IndexReaderOptions& options);

  std::shared_ptr<const SegmentReaderImpl> ReopenColumnStore(
    const Directory& dir, const SegmentMeta& meta,
    const IndexReaderOptions& options) const;
  std::shared_ptr<const SegmentReaderImpl> UpdateMeta(
    const Directory& dir, const SegmentMeta& meta) const;

  uint64_t CountMappedMemory() const final;

  const SegmentInfo& Meta() const final { return _info; }

  ColumnIterator::ptr columns() const final;

  const DocumentMask* docs_mask() const final { return _docs_mask.get(); }

  DocIterator::ptr docs_iterator() const final;

  DocIterator::ptr mask(DocIterator::ptr&& it) const final;

  const TermReader* field(std::string_view name) const final {
    return _field_reader->field(name);
  }

  FieldIterator::ptr fields() const final { return _field_reader->iterator(); }

  const irs::ColumnReader* sort() const noexcept final { return _sort; }

  const irs::ColumnReader* column(field_id field) const final;

  const irs::ColumnReader* column(std::string_view name) const final;

 private:
  using NamedColumns =
    absl::flat_hash_map<std::string_view, const irs::ColumnReader*>;
  using SortedNamedColumns =
    std::vector<std::reference_wrapper<const irs::ColumnReader>>;

  struct ColumnData {
    ColumnstoreReader::ptr columnstore_reader;
    NamedColumns named_columns;
    SortedNamedColumns sorted_named_columns;

    const irs::ColumnReader* Open(const Directory& dir, const SegmentMeta& meta,
                                  const IndexReaderOptions& options,
                                  const FieldReader& field_reader);
  };

  FileRefs _refs;
  SegmentInfo _info;
  std::shared_ptr<const DocumentMask> _docs_mask;
  FieldReader::ptr _field_reader;
  std::shared_ptr<ColumnData> _data;
  // logically part of data_, stored separate to avoid unnecessary indirection
  const irs::ColumnReader* _sort{};
};

}  // namespace irs
