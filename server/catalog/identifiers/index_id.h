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

#include <limits>

#include "basics/identifier.h"

namespace sdb {

class IndexId : public basics::Identifier {
 public:
  using Identifier::Identifier;

  static constexpr IndexId none() {
    return IndexId{std::numeric_limits<BaseType>::max()};
  }

  /// create an id for a primary index
  static constexpr IndexId primary() { return IndexId{0}; }

  /// create an id for an edge _from index
  static constexpr IndexId edgeFrom() { return IndexId{1}; }

  /// create an id for an edge _to index
  static constexpr IndexId edgeTo() { return IndexId{2}; }

  bool isSet() const { return id() != std::numeric_limits<BaseType>::max(); }
  bool isPrimary() const { return id() == 0; }
  bool isEdge() const { return id() == 1 || id() == 2; }
};

static_assert(sizeof(IndexId) == sizeof(IndexId::BaseType));

}  // namespace sdb
