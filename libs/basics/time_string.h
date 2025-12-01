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

#include <absl/time/time.h>

#include <chrono>

inline constexpr std::string_view kTimepointFormat = "%Y-%m-%dT%H:%M:%SZ";

inline std::string TimepointToString(
  const std::chrono::system_clock::time_point& t) {
  return absl::FormatTime(kTimepointFormat,
                          absl::FromChrono(floor<std::chrono::seconds>(t)),
                          absl::UTCTimeZone());
}

inline std::string TimepointToString(
  const std::chrono::system_clock::duration& d) {
  return TimepointToString(std::chrono::system_clock::time_point{} + d);
}

inline std::chrono::system_clock::time_point StringToTimepoint(
  std::string_view s) {
  if (absl::Time t; absl::ParseTime(kTimepointFormat, s, &t, nullptr)) {
    return absl::ToChronoTime(t);
  }
  // TODO(mbkkt) maybe log detailed error?
  return {};
}
