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
#include <string_view>

namespace sdb {

using tp_sys_clock_ms =
  std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>;

namespace basics {

bool ParseDateTime(std::string_view date_time, tp_sys_clock_ms& date_tp);

std::string FormatDate(std::string_view format_string,
                       const tp_sys_clock_ms& date_value);

struct ParsedDuration {
  int years = 0;
  int months = 0;
  int weeks = 0;
  int days = 0;
  int hours = 0;
  int minutes = 0;
  int seconds = 0;
  int milliseconds = 0;
};

bool ParseIsoDuration(std::string_view duration, ParsedDuration& output);

}  // namespace basics
}  // namespace sdb
