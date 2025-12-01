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

#include "basics/bit_utils.hpp"
#include "catalog/object.h"

namespace sdb::catalog {

enum class RoleOption : uint32_t {
  Default = 0,
  Superuser = 1U,
  Inherit = 1U << 1,
  Login = 1U << 2,
  CreateDatabase = 1U << 3,
  CreateRole = 1U << 4,
  BypassRls = 1U << 5,
};

ENABLE_BITMASK_ENUM(RoleOption);

class Role : public Object {
 public:
  Role(ObjectId id, std::string_view name, bool superuser = false)
    : Object{{}, id, name, ObjectType::Role}, _superuser{superuser} {}

  RoleOption GetOptions() const noexcept {
    return (Superuser() ? RoleOption::Superuser : RoleOption::Default) |
           (Inherit() ? RoleOption::Inherit : RoleOption::Default) |
           (CanLogin() ? RoleOption::Login : RoleOption::Default) |
           (CreateDb() ? RoleOption::CreateDatabase : RoleOption::Default) |
           (CreateRole() ? RoleOption::CreateRole : RoleOption::Default) |
           (BypassRls() ? RoleOption::BypassRls : RoleOption::Default);
  }

  bool Superuser() const noexcept { return _superuser; }
  bool Inherit() const noexcept { return _inherit; }
  bool CreateRole() const noexcept { return _create_role; }
  bool CreateDb() const noexcept { return _create_db; }
  bool BypassRls() const noexcept { return _bypass_rls; }
  bool CanLogin() const noexcept { return _can_login; }
  ObjectId Admin() const noexcept { return _admin; }

 private:
  ObjectId _admin = id::kInvalid;
  bool _superuser = true;
  bool _inherit = false;
  bool _create_role = false;
  bool _create_db = false;
  bool _bypass_rls = false;
  bool _can_login = false;
};

}  // namespace sdb::catalog
