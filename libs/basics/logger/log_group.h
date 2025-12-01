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

#include <atomic>
#include <cstddef>

namespace sdb {

class LogGroup {
 public:
  // Number of log groups; must increase this when a new group is added
  // we currently use the following log groups:
  // - 0: default log group: normal logging
  static constexpr size_t kCount = 2;

  constexpr LogGroup(size_t id) : _id{id} {}

  /// Must return a UNIQUE identifier amongst all LogGroup derivatives
  /// and must be less than Count
  size_t id() const noexcept { return _id; }

  /// max length of log entries in this group
  size_t maxLogEntryLength() const noexcept {
    return _max_log_entry_length.load(std::memory_order_relaxed);
  }

  /// set the max length of log entries in this group.
  /// should not be called during the setup of the Logger, and not at runtime
  void maxLogEntryLength(size_t value) { _max_log_entry_length.store(value); }

 private:
  size_t _id;
  std::atomic_size_t _max_log_entry_length{256 * 1'048'576};
};

}  // namespace sdb
