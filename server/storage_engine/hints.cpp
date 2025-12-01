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

#include "storage_engine/hints.h"

#include <iostream>
#include <string>

namespace sdb::transaction {

std::string Hints::toString() const {
  std::string result;

  auto append = [&result](std::string_view value) {
    if (!result.empty()) {
      result.append(", ");
    }
    result.append(value);
  };

  if (has(Hint::SingleOperation)) {
    append("single operation");
  }
  if (has(Hint::LockNever)) {
    append("lock never");
  }
  if (has(Hint::NoIndexing)) {
    append("no indexing");
  }
  if (has(Hint::IntermediateCommits)) {
    append("intermediate commits");
  }
  if (has(Hint::AllowRangeDelete)) {
    append("allow range delete");
  }
  if (has(Hint::FromToplevelAql)) {
    append("from toplevel aql");
  }
  if (has(Hint::GlobalManaged)) {
    append("global managed");
  }
  if (has(Hint::IndexCreation)) {
    append("index creation");
  }
  if (has(Hint::IsFollowerTrx)) {
    append("is follower trx");
  }
  if (has(Hint::AllowFastLockRoundCluster)) {
    append("allow fast lock round cluster");
  }
  if (result.empty()) {
    append("none");
  }

  return result;
}

}  // namespace sdb::transaction
