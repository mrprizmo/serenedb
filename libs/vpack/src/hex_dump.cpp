////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
///
////////////////////////////////////////////////////////////////////////////////

#include "vpack/hex_dump.h"

#include <ostream>

#include "vpack/common.h"

using namespace vpack;

std::string HexDump::toHex(uint8_t value, std::string_view header) {
  std::string result(header);
  appendHex(result, value);
  return result;
}

void HexDump::appendHex(std::string& result, uint8_t value) {
  uint8_t x = value / 16;
  result.push_back((x < 10 ? ('0' + x) : ('a' + x - 10)));
  x = value % 16;
  result.push_back((x < 10 ? ('0' + x) : ('a' + x - 10)));
}

std::string HexDump::toString() const {
  std::string result;
  result.reserve(4 * (length + separator.size()) + (length / values_per_line) +
                 1);

  const uint8_t* p = data;
  const uint8_t* e = p + length;
  int current = 0;
  while (p < e) {
    if (current != 0) {
      result.append(separator);

      if (values_per_line > 0 && current == values_per_line) {
        result.push_back('\n');
        current = 0;
      }
    }

    result.append(header);
    appendHex(result, *p++);
    ++current;
  }

  return result;
}

namespace vpack {

std::ostream& operator<<(std::ostream& stream, const HexDump& hexdump) {
  stream << hexdump.toString();
  return stream;
}

}  // namespace vpack
