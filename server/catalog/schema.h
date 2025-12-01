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

#pragma once

#include "catalog/identifiers/identifier.h"
#include "catalog/object.h"
#include "catalog/view.h"

namespace sdb::catalog {

struct SchemaOptions {
  std::string name;
  ObjectId id;
  ObjectId owner_id;
};

class Schema : public Object {
 public:
  Schema(ObjectId owner_id, ObjectId id, std::string_view name)
    : Object{owner_id, id, name, ObjectType::Schema} {}

  void WriteInternal(vpack::Builder& build) const override {}
};

}  // namespace sdb::catalog
