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

#include <memory>
#include <vector>

namespace sdb {

// This implementation is fine for io heavy workloads but might not be
// sufficient for computational expensive operations with a lot of concurrent
// push/pop operations.
template<typename T>
struct BoundedChannel {
  explicit BoundedChannel(size_t queue_size) : queue(queue_size) {}

  void producerBegin() noexcept {
    absl::MutexLock guard{&mutex};
    num_producer += 1;
  }

  void producerEnd() noexcept {
    absl::MutexLock guard{&mutex};
    num_producer -= 1;
    if (num_producer == 0) {
      // wake up all waiting workers
      stopped = true;
      write_cv.notify_all();
    }
  }

  void stop() {
    absl::MutexLock guard{&mutex};
    stopped = true;
    write_cv.notify_all();
    read_cv.notify_all();
  }

  // Pops an item from the queue. If the channel is stopped, returns nullptr.
  // Second value is true, if the pop call blocked.
  std::pair<std::unique_ptr<T>, bool> pop() noexcept {
    absl::MutexLock guard{&mutex};
    bool blocked = false;
    while (!stopped || consume_index < produce_index) {
      if (consume_index < produce_index) {
        // there is something to eat
        auto ours = std::move(queue[consume_index++ % queue.size()]);
        // notify any pending producers
        read_cv.notify_one();
        return std::make_pair(std::move(ours), blocked);
      } else {
        blocked = true;
        write_cv.Wait(&mutex);
      }
    }
    return std::make_pair(nullptr, blocked);
  }

  // First value is false if the value was pushed. Otherwise true, which means
  // the channel is stopped. Workers should terminate. The second value is true
  // if pushing blocked.
  [[nodiscard]] std::pair<bool, bool> push(std::unique_ptr<T> item) {
    absl::MutexLock guard{&mutex};
    bool blocked = false;
    while (!stopped) {
      if (produce_index < queue.size() + consume_index) {
        // there is space to put something in
        queue[produce_index++ % queue.size()] = std::move(item);
        write_cv.notify_one();
        return std::make_pair(false, blocked);
      } else {
        blocked = true;
        read_cv.Wait(&mutex);
      }
    }
    return std::make_pair(true, blocked);
  }

  absl::Mutex mutex;
  absl::CondVar write_cv;
  absl::CondVar read_cv;
  std::vector<std::unique_ptr<T>> queue;
  bool stopped = false;
  size_t num_producer = 0, consume_index = 0, produce_index = 0;
};

template<typename T>
struct BoundedChannelProducerGuard {
  explicit BoundedChannelProducerGuard(BoundedChannel<T>& ch) : _channel(&ch) {
    _channel->producerBegin();
  }

  BoundedChannelProducerGuard() = default;
  BoundedChannelProducerGuard(BoundedChannelProducerGuard&&) noexcept = default;
  BoundedChannelProducerGuard(const BoundedChannelProducerGuard&) = delete;
  BoundedChannelProducerGuard& operator=(BoundedChannelProducerGuard&&) =
    default;
  BoundedChannelProducerGuard& operator=(const BoundedChannelProducerGuard&) =
    delete;

  ~BoundedChannelProducerGuard() { fire(); }

  void fire() {
    if (_channel) {
      _channel->producerEnd();
      _channel.reset();
    }
  }

 private:
  struct Noop {
    template<typename V>
    void operator()(V*) {}
  };
  std::unique_ptr<BoundedChannel<T>, Noop> _channel = nullptr;
};

}  // namespace sdb
