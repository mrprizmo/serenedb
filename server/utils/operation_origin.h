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
#include <string_view>

namespace sdb::transaction {

// simple helper struct that indicates the origin of an operation.
// we use this to categorize operations for testing memory usage tracking.
struct OperationOrigin {
  // type of transaction
  enum class Type : uint8_t {
    // initiated by user via top-level AQL query
    Aql = 0,
    // initiated by user via REST call/JavaScript console
    Rest = 1,
    // internal operation (statistics, TTL index removals, etc.)
    Internal = 2,
  };

  OperationOrigin(const OperationOrigin&) = default;
  OperationOrigin(OperationOrigin&&) = default;
  OperationOrigin& operator=(const OperationOrigin&) = delete;
  OperationOrigin& operator=(OperationOrigin&&) = delete;

  // must remain valid during the entire lifetime of the OperationOrigin
  const std::string_view description;
  const Type type;

 protected:
  // note: the caller is responsible for ensuring that description
  // stays valid for the entire lifetime of the object
  constexpr OperationOrigin(std::string_view description, Type type) noexcept
    : description(description), type(type) {}
};

struct OperationOriginQuery : OperationOrigin {
  constexpr OperationOriginQuery(std::string_view description) noexcept
    : OperationOrigin(description, OperationOrigin::Type::Aql) {}
};

struct OperationOriginREST : OperationOrigin {
  constexpr OperationOriginREST(std::string_view description) noexcept
    : OperationOrigin(description, OperationOrigin::Type::Rest) {}
};

struct OperationOriginInternal : OperationOrigin {
  constexpr OperationOriginInternal(std::string_view description) noexcept
    : OperationOrigin(description, OperationOrigin::Type::Internal) {}
};

#ifdef SDB_GTEST
struct OperationOriginTestCase : OperationOrigin {
  constexpr OperationOriginTestCase() noexcept
    : OperationOrigin("unit test", OperationOrigin::Type::Internal) {}
};
#endif

}  // namespace sdb::transaction
