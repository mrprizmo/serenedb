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
#include <string>

#include "basics/operating-system.h"
#include "basics/result.h"

#ifdef SERENEDB_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#ifdef SERENEDB_HAVE_GETRLIMIT
namespace sdb {

struct FileDescriptors {
  using ValueType = rlim_t;

  static constexpr ValueType kRequiredMinimum = 1024;
  static constexpr ValueType kMaximumValue =
    std::numeric_limits<ValueType>::max();

  ValueType hard;
  ValueType soft;

  static Result load(FileDescriptors& descriptors);

  static Result store(const FileDescriptors& descriptors);

  static Result adjustTo(ValueType value);

  static ValueType recommendedMinimum();

  static bool isUnlimited(ValueType value);

  static std::string stringify(ValueType value);
};

}  // namespace sdb
#endif
