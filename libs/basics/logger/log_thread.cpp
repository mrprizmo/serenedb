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

#include "log_thread.h"

#include <absl/cleanup/cleanup.h>

#include <tuple>

#include "basics/debugging.h"
#include "basics/logger/appender.h"
#include "basics/logger/logger.h"

namespace sdb::log {

LogThread::LogThread(app::AppServer& server, const std::string& name,
                     uint32_t max_queued_log_messages)
  : Thread(server, name),
    _messages(64),
    _max_queued_log_messages(max_queued_log_messages) {}

LogThread::~LogThread() {
  log::Deactive();

  // make sure there are no memory leaks on uncontrolled shutdown
  MessageEnvelope env{nullptr, nullptr};
  while (_messages.pop(env)) {
    delete env.msg;
  }

  shutdown();
}

bool LogThread::log(LogGroup& group, std::unique_ptr<Message>& message) {
  SDB_ASSERT(message != nullptr);

  SDB_IF_FAILURE("LogThread::log") {
    // simulate a successful logging, but actually don't log anything
    return true;
  }

  bool is_direct_log_level =
    (message->level == LogLevel::FATAL || message->level == LogLevel::ERR ||
     message->level == LogLevel::WARN);

  auto num_messages =
    _pending_messages.fetch_add(1, std::memory_order_relaxed) + 1;

  absl::Cleanup rollback = [&]() noexcept {
    // roll back the counter update
    _pending_messages.fetch_sub(1, std::memory_order_relaxed);
    // we didn't inform logger that we dropped a message!
  };

  if (num_messages >= _max_queued_log_messages && !is_direct_log_level) {
    // log queue is full, and the message is not important enough
    // to push it anyway. this will execute the rollback.
    return false;
  }

  if (!_messages.push({&group, message.get()})) {
    // unable to push message onto the queue.
    // this will also execute the rollback.
    return false;
  }

  std::move(rollback).Cancel();

  // only release message if adding to the queue succeeded
  // otherwise we would leak here
  std::ignore = message.release();

  if (is_direct_log_level) {
    this->flush();
  }
  return true;
}

void LogThread::flush() noexcept {
  int tries = 0;

  while (++tries < 5 && hasMessages()) {
    wakeup();
  }
}

void LogThread::wakeup() noexcept {
  absl::MutexLock guard{&_condition.mutex};
  _condition.cv.notify_one();
}

bool LogThread::hasMessages() const noexcept { return !_messages.empty(); }

void LogThread::run() {
  constexpr uint64_t kInitialWaitTime = 25 * 1000;
  constexpr uint64_t kMaxWaitTime = 100 * 1000;

  uint64_t wait_time = kInitialWaitTime;
  while (!isStopping() && log::IsActive()) {
    bool worked = processPendingMessages();
    if (worked) {
      wait_time = kInitialWaitTime;
    } else {
      wait_time *= 2;
      wait_time = std::min(kMaxWaitTime, wait_time);
    }

    absl::MutexLock guard{&_condition.mutex};
    _condition.cv.WaitWithTimeout(&_condition.mutex,
                                  absl::Microseconds(wait_time));
  }

  processPendingMessages();
}

bool LogThread::processPendingMessages() {
  bool worked = false;
  MessageEnvelope env{nullptr, nullptr};

  while (_messages.pop(env)) {
    _pending_messages.fetch_sub(1, std::memory_order_relaxed);
    worked = true;
    SDB_ASSERT(env.group != nullptr);
    SDB_ASSERT(env.msg != nullptr);
    try {
      log::Appender::log(*env.group, *env.msg);
    } catch (...) {
    }

    delete env.msg;
  }
  return worked;
}

}  // namespace sdb::log
