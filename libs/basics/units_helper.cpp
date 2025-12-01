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

#include "units_helper.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace sdb::options::units_helper {

std::pair<std::string_view, uint64_t> ExtractSuffix(std::string_view value) {
  constexpr uint64_t kOneKiB = 1'024;
  constexpr uint64_t kOneKb = 1'000;

  // note: longer strings must come first in this array!
  constexpr std::array<std::pair<std::string_view, uint64_t>, 30> kUnits = {{
    // three letter units
    {std::string_view{"kib"}, kOneKiB},
    {std::string_view{"KiB"}, kOneKiB},
    {std::string_view{"KIB"}, kOneKiB},
    {std::string_view{"mib"}, kOneKiB * kOneKiB},
    {std::string_view{"MiB"}, kOneKiB * kOneKiB},
    {std::string_view{"MIB"}, kOneKiB * kOneKiB},
    {std::string_view{"gib"}, kOneKiB * kOneKiB * kOneKiB},
    {std::string_view{"GiB"}, kOneKiB * kOneKiB * kOneKiB},
    {std::string_view{"GIB"}, kOneKiB * kOneKiB * kOneKiB},
    {std::string_view{"tib"}, kOneKiB * kOneKiB * kOneKiB * kOneKiB},
    {std::string_view{"TiB"}, kOneKiB * kOneKiB * kOneKiB * kOneKiB},
    {std::string_view{"TIB"}, kOneKiB * kOneKiB * kOneKiB * kOneKiB},
    // two letter units
    {std::string_view{"kb"}, kOneKb},
    {std::string_view{"KB"}, kOneKb},
    {std::string_view{"mb"}, kOneKb * kOneKb},
    {std::string_view{"MB"}, kOneKb * kOneKb},
    {std::string_view{"gb"}, kOneKb * kOneKb * kOneKb},
    {std::string_view{"GB"}, kOneKb * kOneKb * kOneKb},
    {std::string_view{"tb"}, kOneKb * kOneKb * kOneKb * kOneKb},
    {std::string_view{"TB"}, kOneKb * kOneKb * kOneKb * kOneKb},
    // single letter units
    {std::string_view{"k"}, kOneKb},
    {std::string_view{"K"}, kOneKb},
    {std::string_view{"m"}, kOneKb * kOneKb},
    {std::string_view{"M"}, kOneKb * kOneKb},
    {std::string_view{"g"}, kOneKb * kOneKb * kOneKb},
    {std::string_view{"G"}, kOneKb * kOneKb * kOneKb},
    {std::string_view{"t"}, kOneKb * kOneKb * kOneKb * kOneKb},
    {std::string_view{"T"}, kOneKb * kOneKb * kOneKb * kOneKb},
    {std::string_view{"b"}, 1},
    {std::string_view{"B"}, 1},
  }};

  // handle unit suffixes
  for (const auto& unit : kUnits) {
    if (value.ends_with(unit.first)) {
      return {unit.first, unit.second};
    }
  }
  return {"", 1};
}

}  // namespace sdb::options::units_helper
