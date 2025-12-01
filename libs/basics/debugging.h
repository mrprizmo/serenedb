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

#include <cstdlib>
#include <set>
#include <string_view>

/// macro SDB_IF_FAILURE
/// this macro can be used in maintainer mode to make the server fail at
/// certain locations in the C code. The points at which a failure is actually
/// triggered can be defined at runtime using AddFailurePointDebugging().
#ifdef SDB_FAULT_INJECTION

#define SDB_IF_FAILURE(what) if (sdb::ShouldFailDebugging(what))

#else

#define SDB_IF_FAILURE(what) if constexpr (false)

#endif

namespace sdb {

/// intentionally cause a segmentation violation or other failures
#ifdef SDB_FAULT_INJECTION
void TerminateDebugging(std::string_view value);
#else
inline void TerminateDebugging(std::string_view) {}
#endif

/// check whether we should fail at a failure point
#ifdef SDB_FAULT_INJECTION
bool ShouldFailDebugging(std::string_view value) noexcept;
#else
inline constexpr bool ShouldFailDebugging(std::string_view) noexcept {
  return false;
}
#endif

/// add a failure point
#ifdef SDB_FAULT_INJECTION
void AddFailurePointDebugging(std::string_view value);
#else
inline void AddFailurePointDebugging(std::string_view) {}
#endif

/// remove a failure point
#ifdef SDB_FAULT_INJECTION
void RemoveFailurePointDebugging(std::string_view value);
#else
inline void RemoveFailurePointDebugging(std::string_view) {}
#endif

/// clear all failure points
#ifdef SDB_FAULT_INJECTION
void ClearFailurePointsDebugging() noexcept;
#else
inline void ClearFailurePointsDebugging() noexcept {}
#endif

/// return all currently set failure points
#ifdef SDB_FAULT_INJECTION
std::set<std::string> GetFailurePointsDebugging();
#else
inline std::set<std::string> GetFailurePointsDebugging() { return {}; }
#endif

/// returns whether failure point debugging can be used
#ifdef SDB_FAULT_INJECTION
inline constexpr bool CanUseFailurePointsDebugging() { return true; }
#else
inline constexpr bool CanUseFailurePointsDebugging() { return false; }
#endif

}  // namespace sdb

#include "basics/assert.h"
