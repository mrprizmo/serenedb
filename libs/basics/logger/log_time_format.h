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

#include <chrono>
#include <string>

#include "basics/containers/flat_hash_set.h"

namespace sdb {
namespace log_time_formats {

enum class TimeFormat {
  Uptime,
  UptimeMillis,
  UptimeMicros,
  UnixTimestamp,
  UnixTimestampMillis,
  UnixTimestampMicros,
  UTCDateString,
  UTCDateStringMillis,
  UTCDateStringMicros,
  LocalDateString,
};

/// whether or not the specified format is a local one
bool IsLocalFormat(TimeFormat format);

/// whether or not the specified format produces string outputs
/// (in contrast to numeric outputs)
bool IsStringFormat(TimeFormat format);

/// return the name of the default log time format
std::string DefaultFormatName();

/// return the names of all log time formats
containers::FlatHashSet<std::string> GetAvailableFormatNames();

/// derive the time format from the name
TimeFormat FormatFromName(const std::string& name);

/// writes the given time into the given buffer,
/// in the specified format
void WriteTime(std::string& out, TimeFormat format,
               std::chrono::system_clock::time_point tp,
               std::chrono::system_clock::time_point start_tp =
                 std::chrono::system_clock::time_point());

}  // namespace log_time_formats
}  // namespace sdb
