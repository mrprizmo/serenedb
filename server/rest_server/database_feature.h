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

#include <vpack/slice.h>

#include "app/app_feature.h"
#include "rest_server/serened.h"

namespace sdb {

class IOHeartbeatThread;
class StorageEngine;
class ReplicationFeature;

class DatabaseFeature final : public SerenedFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Database"; }

  explicit DatabaseFeature(Server& server);
  ~DatabaseFeature() final;

  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;
  void start() final;
  void stop() final;
  void unprepare() final;
  void prepare() final;

  /// will be called when the recovery phase has run
  /// this will call the engine-specific recoveryDone() procedures
  /// and will execute engine-unspecific operations (such as starting
  /// the replication appliers) for all databases

  /// whether or not the DatabaseFeature has started (and thus has
  /// completely populated its lists of databases and collections from
  /// persistent storage)
  bool started() const noexcept;

  /// register a callback
  ///   if StorageEngine.inRecovery() ->
  ///     call at start of recoveryDone() in parallel with other callbacks
  ///     and fail recovery if callback !ok()
  ///   else ->
  ///     call immediately and return result
  Result registerPostRecoveryCallback(std::function<Result()>&& callback);

  bool isInitiallyEmpty() const noexcept { return _is_initially_empty; }
  bool checkVersion() const noexcept { return _check_version; }
  bool upgrade() const noexcept { return _upgrade; }

  void enableCheckVersion() noexcept { _check_version = true; }
  void enableUpgrade() noexcept { _upgrade = true; }
  void disableUpgrade() noexcept { _upgrade = false; }
  void isInitiallyEmpty(bool value) noexcept { _is_initially_empty = value; }

 private:
  void stopAppliers();

  bool _is_initially_empty{false};
  bool _check_version{false};
  bool _upgrade{false};
  std::atomic_bool _started{false};

  ReplicationFeature* _replication_feature = nullptr;
};

}  // namespace sdb
