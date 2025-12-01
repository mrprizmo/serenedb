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

#include "attribute_name_parser.h"

#include <algorithm>
#include <cstddef>
#include <string_view>

#include "basics/errors.h"
#include "basics/exceptions.h"

namespace sdb::basics {

bool AttributeName::isIdentical(std::span<const AttributeName> lhs,
                                std::span<const AttributeName> rhs,
                                bool ignore_expansion_in_last) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].name != rhs[i].name) {
      return false;
    }
    if (lhs[i].should_expand != rhs[i].should_expand) {
      if (!ignore_expansion_in_last) {
        return false;
      }
      if (i != lhs.size() - 1) {
        return false;
      }
    }
  }

  return true;
}

bool AttributeName::namesMatch(std::span<const AttributeName> lhs,
                               std::span<const AttributeName> rhs) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].name != rhs[i].name) {
      return false;
    }
  }
  return true;
}

bool AttributeName::isIdentical(std::span<const std::vector<AttributeName>> lhs,
                                std::span<const std::vector<AttributeName>> rhs,
                                bool ignore_expansion_in_last) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }

  for (size_t i = 0; i < lhs.size(); ++i) {
    if (!isIdentical(lhs[i], rhs[i],
                     ignore_expansion_in_last && (i == lhs.size() - 1))) {
      return false;
    }
  }

  return true;
}

void ParseAttributeString(std::string_view input,
                          std::vector<AttributeName>& result,
                          bool allow_expansion) {
  bool found_expansion = false;
  size_t parsed_until = 0;
  const size_t length = input.length();

  for (size_t pos = 0; pos < length; ++pos) {
    auto token = input[pos];
    if (token == '[') {
      if (!allow_expansion) {
        SDB_THROW(ERROR_BAD_PARAMETER,
                  "cannot use [*] expansion for this type of index");
      }
      // We only allow attr[*] and attr[*].attr2 as valid patterns
      if (length - pos < 3 || input[pos + 1] != '*' || input[pos + 2] != ']' ||
          (length - pos > 3 && input[pos + 3] != '.')) {
        SDB_THROW(ERROR_SERVER_ATTRIBUTE_PARSER_FAILED,
                  "can only use [*] for indexes");
      }
      if (found_expansion) {
        SDB_THROW(ERROR_BAD_PARAMETER,
                  "cannot use multiple [*] "
                  "expansions for a single index "
                  "field");
      }
      result.emplace_back(input.substr(parsed_until, pos - parsed_until), true);
      found_expansion = true;
      pos += 4;
      parsed_until = pos;
    } else if (token == '.') {
      result.emplace_back(input.substr(parsed_until, pos - parsed_until),
                          false);
      ++pos;
      parsed_until = pos;
    }
  }
  if (parsed_until < length) {
    result.emplace_back(input.substr(parsed_until), false);
  }
}

void AttributeNamesToString(std::span<const AttributeName> input,
                            std::string& result, bool exclude_expansion) {
  SDB_ASSERT(result.empty());

  bool is_first = true;
  for (const auto& it : input) {
    if (!is_first) {
      result += ".";
    }
    is_first = false;
    result += it.name;
    if (!exclude_expansion && it.should_expand) {
      result += "[*]";
    }
  }
}

bool AttributeNamesHaveExpansion(const std::vector<AttributeName>& input) {
  return std::any_of(
    input.begin(), input.end(),
    [](const AttributeName& value) { return value.should_expand; });
}

}  // namespace sdb::basics
