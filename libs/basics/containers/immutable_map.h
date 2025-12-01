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

#include <absl/container/hash_container_defaults.h>

#include <immer/map.hpp>
#include <immer/map_transient.hpp>

#include "immer_memory_policy.h"

namespace sdb::containers {

template<typename K, typename T,
         typename Hash = absl::DefaultHashContainerHash<K>,
         typename Eq = absl::DefaultHashContainerEq<K>>
using ImmutableMap = ::immer::map<K, T, Hash, Eq, immer::serene_memory_policy>;

template<typename K, typename T,
         typename Hash = absl::DefaultHashContainerHash<K>,
         typename Eq = absl::DefaultHashContainerEq<K>>
using ImmutableMapTransient =
  ::immer::map_transient<K, T, Hash, Eq, immer::serene_memory_policy>;

}  // namespace sdb::containers
