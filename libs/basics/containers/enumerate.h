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

#include <ranges>

namespace sdb {

// TODO(mbkkt) replace with std::views::enumerate when it will be available
struct Enumerate : std::ranges::range_adaptor_closure<Enumerate> {
  template<std::ranges::viewable_range R>
  constexpr auto operator()(R&& r) const {
    if constexpr (std::ranges::sized_range<R>) {
      auto d = std::ranges::distance(r);
      return std::views::zip(std::views::iota(0, d), std::forward<R>(r));
    } else {
      return std::views::zip(std::views::iota(0), std::forward<R>(r));
    }
  }
};

inline constexpr Enumerate kEnumerate;

};  // namespace sdb
