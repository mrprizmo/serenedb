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

#include <absl/synchronization/mutex.h>

#include <atomic>
#include <chrono>
#include <cstdint>

namespace sdb::basics {

/// read-write lock, slow but just using CPP11
/// This class has two other advantages:
///  (1) it is possible that a thread tries to acquire a lock even if it
///      has it already. This is important when we are running a thread
///      pool that works on task groups and a task group needs to acquire
///      a lock across multiple (non-concurrent) tasks. This must work,
///      even if tasks from different groups that fight for a lock are
///      actually executed by the same thread! POSIX RW-locks do not have
///      this property.
///  (2) write locks have a preference over read locks: as long as a task
///      wants to get a write lock, no other task can get a (new) read lock.
///      This is necessary to avoid starvation of writers by many readers.
///      The current implementation can starve readers, though.
class ReadWriteLock {
 public:
  void lockWrite();

  [[nodiscard]] bool tryLockWriteFor(std::chrono::microseconds timeout);

  [[nodiscard]] bool tryLockWrite() noexcept;

  void lockRead();

  [[nodiscard]] bool tryLockRead() noexcept;

  [[nodiscard]] bool tryLockReadFor(std::chrono::microseconds timeout);

  void unlockRead() noexcept;

  void unlockWrite() noexcept;

  [[nodiscard]] bool isLocked() const noexcept;
  [[nodiscard]] bool isLockedRead() const noexcept;
  [[nodiscard]] bool isLockedWrite() const noexcept;

 protected:
  /// mutex for _readers_bell cv
  absl::Mutex _reader_mutex;

  /// a condition variable to wake up all reader threads
  absl::CondVar _readers_bell;

  /// mutex for _writers_bell cv
  absl::Mutex _writer_mutex;

  /// a condition variable to wake up one writer thread
  absl::CondVar _writers_bell;

  /// _state, lowest bit is write_lock, the next 31 bits is the number of
  /// queued writers, the last 32 bits the number of active readers.
  std::atomic_uint64_t _state = 0;

  static constexpr uint64_t kWriteLock = 1;

  static constexpr uint64_t kReaderInc = uint64_t{1} << 32;
  static constexpr uint64_t kReaderMask = ~(kReaderInc - 1);

  static constexpr uint64_t kQueuedWriterInc = 1 << 1;
  static constexpr uint64_t kQueuedWriterMask = (kReaderInc - 1) & ~kWriteLock;

  static_assert((kReaderMask & kWriteLock) == 0,
                "READER_MASK and WRITE_LOCK conflict");
  static_assert((kReaderMask & kQueuedWriterMask) == 0,
                "READER_MASK and QUEUED_WRITER_MASK conflict");
  static_assert((kQueuedWriterMask & kWriteLock) == 0,
                "QUEUED_WRITER_MASK and WRITE_LOCK conflict");

  static_assert((kReaderMask & kReaderInc) != 0 &&
                  (kReaderMask & (kReaderInc >> 1)) == 0,
                "READER_INC must be first bit in READER_MASK");
  static_assert((kQueuedWriterMask & kQueuedWriterInc) != 0 &&
                  (kQueuedWriterMask & (kQueuedWriterInc >> 1)) == 0,
                "QUEUED_WRITER_INC must be first bit in QUEUED_WRITER_MASK");
};

}  // namespace sdb::basics
