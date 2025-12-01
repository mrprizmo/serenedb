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
#include <iosfwd>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace sdb::basics {

// Struct that knows the name of the attribute used to identify its pid
// but also knows if the attribute was followed by [*] which means
// it should be expanded. Only works on arrays.
struct AttributeName {
  std::string name;
  bool should_expand;

  AttributeName(std::string_view name, bool expand = false)
    : name(name), should_expand(expand) {}

  AttributeName(std::string name, bool expand)
    : name(std::move(name)), should_expand(expand) {}

  AttributeName(const AttributeName& other) = default;
  AttributeName& operator=(const AttributeName& other) = default;

  AttributeName(AttributeName&& other) = default;
  AttributeName& operator=(AttributeName&& other) = default;

  bool operator==(const AttributeName& other) const = default;

  template<typename H>
  friend H AbslHashValue(H h, const AttributeName& name) {
    return H::combine(std::move(h), name.name, name.should_expand);
  }

  static bool isIdentical(std::span<const AttributeName>,
                          std::span<const AttributeName>, bool) noexcept;

  static bool isIdentical(std::span<const std::vector<AttributeName>> lhs,
                          std::span<const std::vector<AttributeName>> rhs,
                          bool ignore_expansion_in_last) noexcept;

  static bool namesMatch(std::span<const AttributeName>,
                         std::span<const AttributeName>) noexcept;
};

void ParseAttributeString(std::string_view input,
                          std::vector<AttributeName>& result,
                          bool allow_expansion);

void AttributeNamesToString(std::span<const AttributeName> input,
                            std::string& result,
                            bool exclude_expansion = false);

bool AttributeNamesHaveExpansion(const std::vector<AttributeName>& input);

}  // namespace sdb::basics
