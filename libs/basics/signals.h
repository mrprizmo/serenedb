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

#include <string_view>

namespace sdb::signals {

enum class SignalType { Term, Core, Cont, Ign, Logrotate, Stop, User };

/// find out what impact a signal will have to the process we send it.
SignalType MakeSignalType(int signal) noexcept;

/// whether or not the signal is deadly
bool IsDeadly(int signal) noexcept;

/// return the name for a signal
std::string_view Name(int signal) noexcept;

std::string_view SubtypeName(int signal, int sub_code) noexcept;

void MaskAllSignals();
void MaskAllSignalsServer();
void MaskAllSignalsClient();
void UnmaskAllSignals();

}  // namespace sdb::signals
