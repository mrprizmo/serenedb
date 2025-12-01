////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025 SereneDB GmbH, Berlin, Germany
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
/// Copyright holder is SereneDB GmbH, Berlin, Germany
////////////////////////////////////////////////////////////////////////////////

#include "logical_object.h"

#include "basics/assert.h"
#include "basics/down_cast.h"
#include "catalog/function.h"
#include "catalog/identifiers/identifier.h"
#include "catalog/identifiers/object_id.h"
#include "catalog/object.h"
#include "catalog/table.h"
#include "catalog/types.h"
#include "catalog/view.h"
#include "magic_enum/magic_enum.hpp"

namespace sdb::catalog {

namespace {

ObjectType GetObjectType(ObjectCategory category) {
  switch (category) {
    case ObjectCategory::Collection:
      return catalog::ObjectType::Table;
    case ObjectCategory::View:
      return catalog::ObjectType::View;
    case ObjectCategory::Function:
      return catalog::ObjectType::Function;
    case ObjectCategory::Virtual:
      return catalog::ObjectType::View;  // TODO(gnusi): ??
    case ObjectCategory::Role:
      return catalog::ObjectType::Role;
    default:
      SDB_ASSERT(false, magic_enum::enum_name(category));
      return catalog::ObjectType::Invalid;
  }
}

}  // namespace

LogicalObject::LogicalObject(ObjectCategory category, ObjectId database_id,
                             ObjectId id, std::string&& name)
  : SchemaObject{{}, ObjectId{database_id}, id, name, GetObjectType(category)},
    _db_id{database_id},
    _category{category} {}

template<typename V>
bool LogicalObject::Is(V v) const noexcept {
  if constexpr (std::is_same_v<V, TableType>) {
    return category() == ObjectCategory::Collection &&
           basics::downCast<catalog::Table>(*this).GetType() == v;
  } else if constexpr (std::is_same_v<V, ViewType>) {
    return category() == ObjectCategory::View &&
           basics::downCast<catalog::View>(*this).GetViewType() == v;
  } else if constexpr (std::is_same_v<V, FunctionLanguage>) {
    return category() == ObjectCategory::Function &&
           basics::downCast<catalog::Function>(*this).Options().language == v;
  } else {
    static_assert(std::is_same_v<V, ObjectCategory>);
    return category() == v;
  }
}

template bool LogicalObject::Is(ViewType) const noexcept;
template bool LogicalObject::Is(TableType) const noexcept;
template bool LogicalObject::Is(FunctionLanguage) const noexcept;
template bool LogicalObject::Is(ObjectCategory) const noexcept;

}  // namespace sdb::catalog
