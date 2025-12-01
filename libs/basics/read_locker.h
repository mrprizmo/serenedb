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

#include <thread>

#include "basics/common.h"
#include "basics/debugging.h"
#include "basics/locking.h"

/// construct locker with file and line information
#define READ_LOCKER(obj, lock)                                            \
  sdb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
    &lock, sdb::basics::LockerType::BLOCKING, true, __FILE__, __LINE__)

#define READ_LOCKER_EVENTUAL(obj, lock)                                   \
  sdb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
    &lock, sdb::basics::LockerType::EVENTUAL, true, __FILE__, __LINE__)

#define TRY_READ_LOCKER(obj, lock)                                        \
  sdb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
    &lock, sdb::basics::LockerType::TRY, true, __FILE__, __LINE__)

#define CONDITIONAL_READ_LOCKER(obj, lock, condition)                     \
  sdb::basics::ReadLocker<typename std::decay<decltype(lock)>::type> obj( \
    &lock, sdb::basics::LockerType::BLOCKING, (condition), __FILE__, __LINE__)

namespace sdb::basics {

/// read locker
/// A ReadLocker read-locks a read-write lock during its lifetime and unlocks
/// the lock when it is destroyed.
template<typename LockType>
class ReadLocker {
  ReadLocker(const ReadLocker&) = delete;
  ReadLocker& operator=(const ReadLocker&) = delete;
  ReadLocker& operator=(ReadLocker&& other) = delete;

 public:
  /// acquires a read-lock
  /// The constructor acquires a read lock, the destructor unlocks the lock.
  ReadLocker(LockType* read_write_lock, LockerType type, bool condition,
             const char* file, int line) noexcept
    : _read_write_lock(read_write_lock),
      _file(file),
      _line(line),
      _is_locked(false) {
    if (condition) {
      if (type == LockerType::BLOCKING) {
        lock();
        SDB_ASSERT(_is_locked);
      } else if (type == LockerType::EVENTUAL) {
        lockEventual();
        SDB_ASSERT(_is_locked);
      } else if (type == LockerType::TRY) {
        _is_locked = tryLock();
      }
    }
  }

  ReadLocker(ReadLocker&& other) noexcept
    : _read_write_lock(other._read_write_lock),
      _file(other._file),
      _line(other._line),
      _is_locked(other._is_locked) {
    // make only ourselves responsible for unlocking
    other.steal();
  }

  /// releases the read-lock
  ~ReadLocker() {
    if (_is_locked) {
      _read_write_lock->unlockRead();
    }
  }

  /// whether or not we acquired the lock
  bool isLocked() const noexcept { return _is_locked; }

  /// eventually acquire the read lock
  void lockEventual() noexcept {
    while (!tryLock()) {
      std::this_thread::yield();
    }
    SDB_ASSERT(_is_locked);
  }

  bool tryLock() noexcept {
    SDB_ASSERT(!_is_locked);
    if (_read_write_lock->tryLockRead()) {
      _is_locked = true;
    }
    return _is_locked;
  }

  /// acquire the read lock, blocking
  void lock() noexcept {
    SDB_ASSERT(!_is_locked);
    _read_write_lock->lockRead();
    _is_locked = true;
  }

  /// unlocks the lock if we own it
  bool unlock() noexcept {
    if (_is_locked) {
      _read_write_lock->unlockRead();
      _is_locked = false;
      return true;
    }
    return false;
  }

  /// steals the lock, but does not unlock it
  bool steal() noexcept {
    if (_is_locked) {
      _is_locked = false;
      return true;
    }
    return false;
  }

 private:
  /// the read-write lock
  LockType* _read_write_lock;

  /// file
  const char* _file;

  /// line number
  int _line;

  /// whether or not we acquired the lock
  bool _is_locked;
};

}  // namespace sdb::basics
