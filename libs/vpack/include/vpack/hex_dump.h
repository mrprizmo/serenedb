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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>

#include "vpack/common.h"
#include "vpack/slice.h"
#include "vpack/value.h"
#include "vpack/value_type.h"

namespace vpack {

struct HexDump {
  HexDump() = delete;

  HexDump(Slice slice, int values_per_line = 16,
          const std::string& separator = " ", const std::string& header = "0x")
    : data(slice.start()),
      length(slice.byteSize()),
      values_per_line(values_per_line),
      separator(separator),
      header(header) {}

  HexDump(const uint8_t* data, ValueLength length, int values_per_line = 16,
          const std::string& separator = " ", const std::string& header = "0x")
    : data(data),
      length(length),
      values_per_line(values_per_line),
      separator(separator),
      header(header) {}

  static std::string toHex(uint8_t value, std::string_view header = "0x");
  static void appendHex(std::string& result, uint8_t value);
  std::string toString() const;

  friend std::ostream& operator<<(std::ostream&, const HexDump&);

  const uint8_t* data;
  ValueLength length;
  int values_per_line;
  std::string separator;
  std::string header;
};

}  // namespace vpack
