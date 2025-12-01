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

#include <memory>
#include <string_view>

#include "basics/common.h"
#include "basics/memory.hpp"

namespace sdb {

class QueryString {
 public:
  QueryString() = default;
  QueryString(const QueryString& other) = delete;
  QueryString& operator=(const QueryString& other) = delete;
  QueryString(QueryString&& other) = default;
  QueryString& operator=(QueryString&& other) = default;

  explicit QueryString(std::string_view val);
  std::string_view view() const noexcept { return {_str.get(), size()}; }
  const char* data() const noexcept { return _str.get(); }
  size_t size() const noexcept {
    // -2 becuase of two null terminators
    return _str.get_deleter().size() - 2;
  }
  bool empty() const noexcept { return size() == 0; }
  uint64_t hash() const noexcept { return _hash; }
  std::string extract(size_t max_length) const;
  std::string extractRegion(int line, int column) const;
  void reset() noexcept;

 private:
  using Allocator = std::allocator<char>;
  using Deleter = irs::memory::AllocatorArrayDeallocator<Allocator>;
  using Str = std::unique_ptr<char[], Deleter>;

  Str _str{nullptr, {Allocator{}, 0}};
  uint64_t _hash{};
};

}  // namespace sdb
