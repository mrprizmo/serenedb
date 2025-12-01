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

#include <vpack/builder.h>
#include <vpack/slice.h>

#include <cstdint>
#include <map>

#include "basics/result.h"
#include "catalog/identifiers/object_id.h"

namespace sdb {
namespace methods {

/// not based on basics/Result because I did not
/// feel like adding these error codes globally.
/// Originally from js/server/database-version.js
struct VersionResult {
  enum StatusCode : int {
    kInvalid = 0,
    kVersionMatch = 1,
    kUpgradeNeeded = 2,
    kDowngradeNeeded = 3,
    kCannotParseVersionFile = -2,
    kCannotReadVersionFile = -3,
    kNoVersionFile = -4,
    kNoServerVersion = -5
  };

  /// status code
  StatusCode status;
  /// current server version
  uint64_t server_version;
  /// version in VERSION file on disk
  uint64_t database_version;
  /// tasks that were executed
  std::map<std::string, bool> tasks;
};

/// Code to create and initialize databases
/// Replaces upgrade-database.js for good
struct Version {
  /// "(((major * 100) + minor) * 100) + patch"
  static uint64_t current();
  /// read the VERSION file for a database
  static VersionResult check(ObjectId database);
  static VersionResult::StatusCode compare(uint64_t current, uint64_t other);
  /// write a VERSION file including all tasks
  static Result write(ObjectId database,
                      const std::map<std::string, bool>& tasks, bool sync);

  static uint64_t parseVersion(std::string_view version);
};

}  // namespace methods
}  // namespace sdb
