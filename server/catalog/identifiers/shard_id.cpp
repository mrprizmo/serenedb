////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

#include "shard_id.h"

#include <absl/strings/str_cat.h>

#include "basics/exceptions.h"
#include "basics/string_utils.h"

namespace sdb {

ResultOr<ShardID> ShardID::shardIdFromString(std::string_view s) {
  if (!s.starts_with('s')) {
    return std::unexpected<Result>{std::in_place, ERROR_BAD_PARAMETER,
                                   "Expected ShardID to start with 's'"};
  }
  auto res = basics::string_utils::TryUint64(s.data() + 1, s.length() - 1);
  if (!res) {
    return std::unexpected{std::move(res).error()};
  }
  return ShardID{*res};
}

ShardID::ShardID(std::string_view id) {
  auto maybe_shard_id = shardIdFromString(id);
  if (!maybe_shard_id) {
    SDB_THROW(std::move(maybe_shard_id).error());
  }
  *this = *maybe_shard_id;
}

std::string ShardID::toStr() const { return absl::StrCat("s", _id); }

// NOTE: This is not noexcept because of shardIdFromString
bool ShardID::operator==(std::string_view other) const {
  if (auto other_shard = shardIdFromString(other); other_shard) {
    return *this == *other_shard;
  }
  // If the other is not a valid Shard we cannot be equal to it.
  return false;
}

}  // namespace sdb
