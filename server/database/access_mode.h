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

#include <string_view>

#include "basics/common.h"
#include "basics/exceptions.h"

namespace sdb {

struct AccessMode {
  enum class Type {
    None = 0,
    Read = 1,
    Write = 2,
    Exclusive = 4,
  };
  // no need to create an object of it
  AccessMode() = delete;

  static_assert(AccessMode::Type::None < AccessMode::Type::Read &&
                  AccessMode::Type::Read < AccessMode::Type::Write &&
                  AccessMode::Type::Write < AccessMode::Type::Exclusive,
                "AccessMode::Type total order fail");

  static bool isNone(Type type) noexcept { return type == Type::None; }

  static bool isRead(Type type) noexcept { return type == Type::Read; }

  static bool isWrite(Type type) noexcept { return type == Type::Write; }

  static bool isExclusive(Type type) noexcept {
    return type == Type::Exclusive;
  }

  static bool isWriteOrExclusive(Type type) noexcept {
    return isWrite(type) || isExclusive(type);
  }

  /// checks if the type of the two modes is different
  /// this will intentially treat EXCLUSIVE the same as WRITE
  static bool isReadWriteChange(Type lhs, Type rhs) {
    return ((isWriteOrExclusive(lhs) && !isWriteOrExclusive(rhs)) ||
            (!isWriteOrExclusive(lhs) && isWriteOrExclusive(rhs)));
  }

  /// get the transaction type from a string
  static Type fromString(std::string_view value) {
    if (value == "read") {
      return Type::Read;
    }
    if (value == "write") {
      return Type::Write;
    }
    if (value == "exclusive") {
      return Type::Exclusive;
    }
    SDB_THROW(ERROR_INTERNAL, "invalid access type");
  }

  /// return the type of the transaction as a string
  static std::string_view typeString(Type value) {
    switch (value) {
      case Type::None:
        return "none";
      case Type::Read:
        return "read";
      case Type::Write:
        return "write";
      case Type::Exclusive:
        return "exclusive";
    }
    SDB_THROW(ERROR_INTERNAL, "invalid access type");
  }
};

}  // namespace sdb
