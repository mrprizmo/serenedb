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

#include "read_write_lock.h"

#include "basics/debugging.h"

namespace sdb::basics {

void ReadWriteLock::lockWrite() {
  if (tryLockWrite()) {
    return;
  }

  // the lock is either held by another writer or we have active readers
  // -> announce that we want to write
  _state.fetch_add(kQueuedWriterInc, std::memory_order_relaxed);

  absl::MutexLock guard{&_writer_mutex};
  while (true) {
    // it is intentional to reload _state after acquiring the mutex,
    // because in case we were blocked (because someone else was holding
    // the mutex), _state most likely has changed, causing the subsequent
    // CAS to fail. If we were not blocked, then the additional load will
    // most certainly hit the L1 cache and should therefore be very cheap.
    auto state = _state.load(std::memory_order_relaxed);
    // try to acquire write lock as long as no readers or writers are active,
    while ((state & ~kQueuedWriterMask) == 0) {
      // try to acquire lock and perform queued writer decrement in one step
      if (_state.compare_exchange_weak(state,
                                       (state - kQueuedWriterInc) | kWriteLock,
                                       std::memory_order_acquire)) {
        return;
      }
    }
    _writers_bell.Wait(&_writer_mutex);
  }
}

bool ReadWriteLock::tryLockWriteFor(std::chrono::microseconds timeout) {
  if (tryLockWrite()) {
    return true;
  }

  // the lock is either held by another writer or we have active readers
  // -> announce that we want to write
  _state.fetch_add(kQueuedWriterInc, std::memory_order_relaxed);

  auto end_time = absl::Now() + absl::FromChrono(timeout);

  {
    absl::MutexLock guard{&_writer_mutex};
    do {
      auto state = _state.load(std::memory_order_relaxed);
      // try to acquire write lock as long as no readers or writers are active,
      while ((state & ~kQueuedWriterMask) == 0) {
        // try to acquire lock and perform queued writer decrement in one step
        if (_state.compare_exchange_weak(
              state, (state - kQueuedWriterInc) | kWriteLock,
              std::memory_order_acquire)) {
          return true;
        }
      }
    } while (!_writers_bell.WaitWithDeadline(&_writer_mutex, end_time));
  }

  // Undo the counting of us as queued writer:
  auto state = _state.fetch_sub(kQueuedWriterInc, std::memory_order_relaxed) -
               kQueuedWriterInc;

  if ((state & kWriteLock) == 0) {
    if ((state & kQueuedWriterMask) != 0) {
      absl::MutexLock guard{&_writer_mutex};
      _writers_bell.notify_one();
    } else {
      absl::MutexLock guard{&_reader_mutex};
      _readers_bell.notify_all();
    }
  }

  return false;
}

bool ReadWriteLock::tryLockWrite() noexcept {
  // order_relaxed is an optimization, cmpxchg will synchronize side-effects
  auto state = _state.load(std::memory_order_relaxed);
  // try to acquire write lock as long as no readers or writers are active,
  // we might "overtake" other queued writers though.
  while ((state & ~kQueuedWriterMask) == 0) {
    if (_state.compare_exchange_weak(state, state | kWriteLock,
                                     std::memory_order_acquire)) {
      return true;  // we successfully acquired the write lock!
    }
  }
  return false;
}

void ReadWriteLock::lockRead() {
  if (tryLockRead()) {
    return;
  }

  absl::MutexLock guard{&_reader_mutex};
  while (true) {
    if (tryLockRead()) {
      return;
    }

    _readers_bell.Wait(&_reader_mutex);
  }
}

bool ReadWriteLock::tryLockReadFor(std::chrono::microseconds timeout) {
  if (tryLockRead()) {
    return true;
  }
  auto end_time = absl::Now() + absl::FromChrono(timeout);
  absl::MutexLock guard{&_reader_mutex};
  do {
    if (tryLockRead()) {
      return true;
    }
  } while (!_readers_bell.WaitWithDeadline(&_reader_mutex, end_time));
  return false;
}

bool ReadWriteLock::tryLockRead() noexcept {
  // order_relaxed is an optimization, cmpxchg will synchronize side-effects
  auto state = _state.load(std::memory_order_relaxed);
  // try to acquire read lock as long as no writers are active or queued
  while ((state & ~kReaderMask) == 0) {
    if (_state.compare_exchange_weak(state, state + kReaderInc,
                                     std::memory_order_acquire)) {
      return true;
    }
  }
  return false;
}

/// Note that, theoretically, locking a mutex may throw. But in this case, we'd
/// be running into undefined behaviour, so we still want noexcept!
void ReadWriteLock::unlockWrite() noexcept {
  SDB_ASSERT((_state.load() & kWriteLock) != 0);
  // clear the WRITE_LOCK flag
  auto state = _state.fetch_sub(kWriteLock, std::memory_order_release);
  if ((state & kQueuedWriterMask) != 0) {
    // there are other writers waiting -> wake up one of them
    absl::MutexLock guard{&_writer_mutex};
    _writers_bell.notify_one();
  } else {
    // no more writers -> wake up any waiting readings
    absl::MutexLock guard{&_reader_mutex};
    _readers_bell.notify_all();
  }
}

/// Note that, theoretically, locking a mutex may throw. But in this case, we'd
/// be running into undefined behaviour, so we still want noexcept!
void ReadWriteLock::unlockRead() noexcept {
  SDB_ASSERT((_state.load() & kReaderMask) != 0);
  auto state =
    _state.fetch_sub(kReaderInc, std::memory_order_release) - kReaderInc;
  if (state != 0 && (state & ~kQueuedWriterMask) == 0) {
    // we were the last reader and there are other writers waiting
    // -> wake up one of them
    absl::MutexLock guard{&_writer_mutex};
    _writers_bell.notify_one();
  }
}

bool ReadWriteLock::isLocked() const noexcept {
  return (_state.load(std::memory_order_relaxed) & ~kQueuedWriterMask) != 0;
}

bool ReadWriteLock::isLockedRead() const noexcept {
  return (_state.load(std::memory_order_relaxed) & kReaderMask) > 0;
}

bool ReadWriteLock::isLockedWrite() const noexcept {
  return _state.load(std::memory_order_relaxed) & kWriteLock;
}

}  // namespace sdb::basics
