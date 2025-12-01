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

#include "query_string.h"

#include <vpack/common.h>

#include "basics/debugging.h"
#include "basics/string_utils.h"

namespace sdb {
namespace {

uint64_t ComputeHash(std::string_view data) noexcept {
  if (data.empty()) {
    return 0;
  }

  return VPACK_HASH(data.data(), data.size(), 0x3123456789abcdef);
}

}  // namespace

QueryString::QueryString(std::string_view val)
  : _str{irs::memory::AllocateUnique<char[]>(Allocator{}, val.size() + 2,
                                             irs::memory::kAllocateOnly)},
    _hash{ComputeHash(val)} {
  // Sql query requires double null terminator
  std::memcpy(_str.get(), val.data(), val.size());
  _str[val.size()] = '\0';
  _str[val.size() + 1] = '\0';
}

void QueryString::reset() noexcept {
  _str.reset();
  _hash = 0;
}

std::string QueryString::extract(size_t max_length) const {
  if (size() <= max_length) {
    // no truncation
    return {data(), size()};
  }

  // query string needs truncation
  size_t length = max_length;

  // do not create invalid UTF-8 sequences
  while (length > 0) {
    uint8_t c = _str[length - 1];
    if ((c & 128) == 0) {
      // single-byte character
      break;
    }
    --length;

    // part of a multi-byte sequence
    if ((c & 192) == 192) {
      // decrease length by one more, so the string contains the
      // last part of the previous (multi-byte?) sequence
      break;
    }
  }

  std::string result;
  result.reserve(length + 15);
  result.append(data(), length);
  result.append("... (", 5);
  basics::string_utils::Itoa(size() - length, result);
  result.append(")", 1);
  return result;
}

/// extract a region from the query
std::string QueryString::extractRegion(int line, int column) const {
  // note: line numbers reported by bison/flex start at 1, columns start at 0
  int current_line = 1;
  int current_column = 0;

  char c;
  const char* s = data();
  const char* p = data();
  const size_t n = size();

  while ((static_cast<size_t>(p - s) < n) && (c = *p)) {
    if (current_line > line ||
        (current_line >= line && current_column >= column)) {
      break;
    }

    if (c == '\n') {
      ++p;
      ++current_line;
      current_column = 0;
    } else if (c == '\r') {
      ++p;
      ++current_line;
      current_column = 0;

      // eat a following newline
      if (*p == '\n') {
        ++p;
      }
    } else {
      ++current_column;
      ++p;
    }
  }

  // p is pointing at the position in the query the parse error occurred at
  SDB_ASSERT(p >= s);

  size_t offset = static_cast<size_t>(p - s);

  constexpr int kSnippetLength = 32;

  // copy query part, UTF-8-aware
  std::string result;
  result.reserve(kSnippetLength + 3 /*...*/);

  {
    const char* start = s + offset;
    const char* end = s + size();

    int chars_found = 0;

    while (start < end) {
      char c = *start;

      if ((c & 128) == 0 || (c & 192) == 192) {
        // ASCII character or start of a multi-byte sequence
        ++chars_found;
        if (chars_found > kSnippetLength) {
          break;
        }
      }

      result.push_back(c);
      ++start;
    }

    if (start != end) {
      result.append("...");
    }
  }

  return result;
}

}  // namespace sdb
