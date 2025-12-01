////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025 SereneDB GmbH, Berlin, Germany
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
/// Copyright holder is SereneDB GmbH, Berlin, Germany
////////////////////////////////////////////////////////////////////////////////

#include "pg/pg_catalog/pg_namespace.h"

#include "pg/pg_catalog/fwd.h"

namespace sdb::pg {
namespace {

constexpr auto kSampleData = std::to_array<PgNamespace>({
  {
    .oid = 11,
    .nspname = "pg_catalog",
  },
  {
    .oid = 2200,
    .nspname = "public",
  },
});

constexpr uint64_t kNullMask = MaskFromNonNulls({
  GetIndex(&PgNamespace::oid),
  GetIndex(&PgNamespace::nspname),
});

}  // namespace

template<>
std::vector<velox::VectorPtr> SystemTableSnapshot<PgNamespace>::GetTableData(
  velox::memory::MemoryPool& pool) {
  std::vector<velox::VectorPtr> result;
  result.reserve(boost::pfr::tuple_size_v<PgNamespace>);
  boost::pfr::for_each_field(
    PgNamespace{}, [&]<typename Field>(const Field& field) {
      auto column = CreateColumn<Field>(kSampleData.size(), &pool);
      result.push_back(std::move(column));
    });
  for (size_t row = 0; row < kSampleData.size(); ++row) {
    WriteData(result, kSampleData[row], kNullMask, row, &pool);
  }
  return result;
}
}  // namespace sdb::pg
