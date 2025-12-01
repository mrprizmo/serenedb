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

#include "cpu_usage_feature.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "app/app_server.h"
#include "basics/debugging.h"

namespace sdb {

#if defined(__linux__)
struct CpuUsageFeature::SnapshotProvider {
  SnapshotProvider();
  ~SnapshotProvider();
  SnapshotProvider(const SnapshotProvider&) = delete;
  SnapshotProvider& operator=(const SnapshotProvider&) = delete;

  bool CanTakeSnapshot() const noexcept { return _stat_file != nullptr; }

  bool TryTakeSnapshot(CpuUsageSnapshot& result) noexcept;

 private:
  size_t ReadStatFile(char* buffer, size_t buffer_size) noexcept;

  /// handle for /proc/stat
  FILE* _stat_file;
};

CpuUsageFeature::SnapshotProvider::SnapshotProvider() : _stat_file(nullptr) {
  // we are opening the /proc/stat file only once during the lifetime of the
  // process, in order to avoid frequent open/close calls
  _stat_file = fopen("/proc/stat", "r");
}

CpuUsageFeature::SnapshotProvider::~SnapshotProvider() {
  if (_stat_file != nullptr) {
    fclose(_stat_file);
  }
}

bool CpuUsageFeature::SnapshotProvider::TryTakeSnapshot(
  CpuUsageSnapshot& result) noexcept {
  constexpr size_t kBufferSize = 4096;

  // none of the following methods will throw an exception
  rewind(_stat_file);
  fflush(_stat_file);

  char buffer[kBufferSize];
  buffer[0] = '\0';

  size_t nread = ReadStatFile(&buffer[0], kBufferSize);
  // expect a minimum size
  if (nread < 32 || memcmp(&buffer[0], "cpu ", 4) != 0) {
    // invalid data read.
    return false;
  }

  // 4 bytes because we skip the initial "cpu " intro
  result = CpuUsageSnapshot::fromString(&buffer[4], kBufferSize - 4);
  return true;
}

size_t CpuUsageFeature::SnapshotProvider::ReadStatFile(
  char* buffer, size_t buffer_size) noexcept {
  size_t offset = 0;
  size_t remain = buffer_size - 1;
  while (remain > 0) {
    SDB_ASSERT(offset < buffer_size);

    size_t nread = fread(buffer + offset, 1, remain, _stat_file);
    if (nread == 0) {
      break;
    }
    remain -= nread;
    offset += nread;
  }
  SDB_ASSERT(offset < buffer_size);
  buffer[offset] = '\0';
  return offset;
}
#else
struct CpuUsageFeature::SnapshotProvider {
  bool canTakeSnapshot() const noexcept { return false; }

  bool tryTakeSnapshot(CpuUsageSnapshot&) noexcept {
    SDB_ASSERT(false);  // should never be called!
    return false;
  }
};
#endif

CpuUsageFeature::CpuUsageFeature(Server& server)
  : SerenedFeature{server, name()},
    _snapshot_provider(),
    _update_in_progress(false) {
  setOptional(true);
}

void CpuUsageFeature::prepare() {
  _snapshot_provider = std::make_unique<SnapshotProvider>();

  if (!_snapshot_provider->CanTakeSnapshot()) {
    // we will not be able to provide any stats, so let's disable ourselves
    disable();
  }
}

CpuUsageFeature::~CpuUsageFeature() = default;

CpuUsageSnapshot CpuUsageFeature::snapshot() {
  CpuUsageSnapshot last_snapshot, last_delta;

  if (!isEnabled()) {
    return last_delta;
  }

  // whether or not a concurrent thread is currently updating our
  // snapshot. if this is the case, we will simply return the old
  // snapshot
  bool update_in_progress;
  {
    // read last snapshot under the mutex
    std::lock_guard guard{_snapshot_mutex};
    last_snapshot = _snapshot;
    last_delta = _snapshot_delta;
    update_in_progress = _update_in_progress;

    if (!update_in_progress) {
      // its our turn!
      _update_in_progress = true;
    }
  }

  // now we can go on with a copy of the last snapshot without
  // holding the mutex
  if (update_in_progress) {
    // in a multi-threaded environment, we need to serialize our access
    // to /proc/stat by multiple concurrent threads. this also helps
    // reducing the load if multiple threads concurrently request the
    // CPU statistics
    return last_delta;
  }

  CpuUsageSnapshot next;
  auto success = _snapshot_provider->TryTakeSnapshot(next);
  {
    // snapshot must be updated and returned under mutex
    std::lock_guard guard{_snapshot_mutex};
    if (success) {
      // if we failed to obtain new snapshot, we simply return whatever we had
      // before
      _snapshot = next;
      if (last_snapshot.valid()) {
        next.subtract(last_snapshot);
      }
      _snapshot_delta = next;
    }
    _update_in_progress = false;
    return _snapshot_delta;
  }
}

}  // namespace sdb
