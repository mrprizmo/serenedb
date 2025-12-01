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

#include <string_view>

namespace sdb::colors {

inline constexpr std::string_view kRed = "\x1b[31m";
inline constexpr std::string_view kBoldRed = "\x1b[1;31m";
inline constexpr std::string_view kGreen = "\x1b[32m";
inline constexpr std::string_view kBoldGreen = "\x1b[1;32m";
inline constexpr std::string_view kBlue = "\x1b[34m";
inline constexpr std::string_view kBoldBlue = "\x1b[1;34m";
inline constexpr std::string_view kYellow = "\x1b[33m";
inline constexpr std::string_view kBoldYellow = "\x1b[1;33m";
inline constexpr std::string_view kWhite = "\x1b[37m";
inline constexpr std::string_view kBoldWhite = "\x1b[1;37m";
inline constexpr std::string_view kBlack = "\x1b[30m";
inline constexpr std::string_view kBoldBlack = "\x1b[1;30m";
inline constexpr std::string_view kCyan = "\x1b[36m";
inline constexpr std::string_view kBoldCyan = "\x1b[1;36m";
inline constexpr std::string_view kMagenta = "\x1b[35m";
inline constexpr std::string_view kBoldMagenta = "\x1b[1;35m";
inline constexpr std::string_view kBlink = "\x1b[5m";
inline constexpr std::string_view kBright = "\x1b[1m";
inline constexpr std::string_view kReset = "\x1b[0m";
inline constexpr std::string_view kLinkStart = "\x1b]8;;";
inline constexpr std::string_view kLinkMiddle = "\x1b\\";
inline constexpr std::string_view kLinkEnd = "\x1b]8;;\x1b\\";

}  // namespace sdb::colors
