////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include <list>
#include <memory>
#include <mutex>
#include <source_location>
#include <yaclib/async/contract.hpp>
#include <yaclib/async/future.hpp>
#include <yaclib/async/make.hpp>
#include <yaclib/async/promise.hpp>

#include "basics/assert.h"
#include "basics/exceptions.h"

namespace sdb {

template<typename Scheduler>
struct FutureSharedLock {
 private:
  struct Node;
  struct SharedState;

 public:
  struct LockGuard {
    LockGuard() = default;

    LockGuard(const LockGuard&) = delete;
    LockGuard& operator=(const LockGuard&) = delete;

    LockGuard(LockGuard&& r) noexcept : _lock(r._lock) { r._lock = nullptr; }

    LockGuard& operator=(LockGuard&& r) noexcept {
      if (&r != this) {
        _lock = r._lock;
        r._lock = nullptr;
      }
      return *this;
    }

    bool isLocked() const noexcept { return _lock != nullptr; }

    void release() noexcept { _lock = nullptr; }

    void unlock() noexcept {
      SDB_ASSERT(_lock != nullptr);
      _lock->unlock();
      _lock = nullptr;
    }

    ~LockGuard() {
      if (_lock) {
        _lock->unlock();
      }
    }

   private:
    friend struct FutureSharedLock;
    explicit LockGuard(SharedState* lock) : _lock(lock) {}
    SharedState* _lock = nullptr;
  };

  using FutureType = yaclib::Future<LockGuard>;

  explicit FutureSharedLock(Scheduler& scheduler)
    : _shared_state(std::make_shared<SharedState>(scheduler)) {}

  FutureType asyncLockShared() {
    return _shared_state->asyncLockShared([](auto) {});
  }

  FutureType asyncLockExclusive() {
    return _shared_state->asyncLockExclusive([](auto) {});
  }

  FutureType asyncTryLockSharedFor(std::chrono::milliseconds timeout) {
    return _shared_state->asyncLockShared([this, timeout](auto node) {
      _shared_state->scheduleTimeout(node, timeout);
    });
  }

  FutureType asyncTryLockExclusiveFor(std::chrono::milliseconds timeout) {
    return _shared_state->asyncLockExclusive([this, timeout](auto node) {
      _shared_state->scheduleTimeout(node, timeout);
    });
  }

  LockGuard tryLockShared() { return _shared_state->tryLockShared(); }

  LockGuard tryLockExclusive() { return _shared_state->tryLockExclusive(); }

  void unlock() { _shared_state->unlock(); }

 private:
  struct Node {
    explicit Node(yaclib::Promise<LockGuard>&& p, bool exclusive)
      : promise(std::move(p)), exclusive(exclusive) {}

    yaclib::Promise<LockGuard> promise;
    typename Scheduler::WorkHandle work_item;
    bool exclusive;
  };

  struct SharedState : std::enable_shared_from_this<SharedState> {
    explicit SharedState(Scheduler& scheduler) : scheduler(scheduler) {}

    LockGuard tryLockShared() {
      std::lock_guard lock(mutex);
      if (lock_count == 0 || (!exclusive && queue.empty())) {
        ++lock_count;
        exclusive = false;
        return LockGuard{this};
      }

      return {};
    }

    LockGuard tryLockExclusive() {
      std::lock_guard lock(mutex);
      if (lock_count == 0) {
        SDB_ASSERT(queue.empty());
        ++lock_count;
        exclusive = true;
        return LockGuard{this};
      }

      return {};
    }

    template<typename Func>
    FutureType asyncLockExclusive(Func blocked_func) {
      auto [f, p] = yaclib::MakeContract<LockGuard>();
      std::lock_guard lock(mutex);
      if (lock_count == 0) {
        SDB_ASSERT(queue.empty());
        ++lock_count;
        exclusive = true;
        std::move(p).Set(LockGuard{this});
      } else {
        auto it = insertNode(std::move(p), true);
        blocked_func(it);
      }
      return std::move(f);
    }

    template<typename Func>
    FutureType asyncLockShared(Func blocked_func) {
      auto [f, p] = yaclib::MakeContract<LockGuard>();
      std::lock_guard lock(mutex);
      if (lock_count == 0 || (!exclusive && queue.empty())) {
        ++lock_count;
        exclusive = false;
        std::move(p).Set(LockGuard{this});
      } else {
        auto it = insertNode(std::move(p), false);
        blocked_func(it);
      }
      return std::move(f);
    }

    void unlock() {
      std::lock_guard lock(mutex);
      SDB_ASSERT(lock_count > 0);
      if (--lock_count == 0 && !queue.empty()) {
        // we were the last lock holder -> schedule the next node
        auto& node = queue.back();
        lock_count = 1;
        exclusive = node->exclusive;
        scheduleNode(*node);
        queue.pop_back();

        // if we are in shared mode, we can schedule all following shared
        // nodes
        if (!exclusive) {
          while (!queue.empty() && !queue.back()->exclusive) {
            ++lock_count;
            scheduleNode(*queue.back());
            queue.pop_back();
          }
        }
      }
    }

    auto insertNode(yaclib::Promise<LockGuard> p, bool exclusive) {
      queue.emplace_front(std::make_shared<Node>(std::move(p), exclusive));
      return queue.begin();
    }

    void removeNode(
      typename std::list<std::shared_ptr<Node>>::iterator queue_iterator) {
      SDB_ASSERT(!queue.empty());
      SDB_ASSERT(lock_count > 0);
      if (std::addressof(queue.back()) == std::addressof(*queue_iterator)) {
        // we are the first entry in the list -> remove ourselves and check if
        // we can schedule the next node(s)
        queue.pop_back();
        if (!exclusive) {
          while (!queue.empty() && !queue.back()->exclusive) {
            ++lock_count;
            scheduleNode(*queue.back());
            queue.pop_back();
          }
        }
      } else {
        // we are somewhere in the middle -> just remove ourselves
        queue.erase(queue_iterator);
      }
    }

    void scheduleNode(Node& node) {
      // TODO in theory `this` can die before execute callback
      //  so probably better to use `shared_from_this()`, but it's slower
      scheduler.queue([promise = std::move(node.promise), this]() mutable {
        std::move(promise).Set(LockGuard{this});
      });
    }

    void scheduleTimeout(
      typename std::list<std::shared_ptr<Node>>::iterator queue_iterator,
      std::chrono::milliseconds timeout) {
      (*queue_iterator)->work_item = scheduler.queueDelayed(
        [self = this->weak_from_this(),
         node = std::weak_ptr<Node>(*queue_iterator),
         queue_iterator](bool cancelled) mutable {
          if (auto me = self.lock(); me) {
            if (auto node_ptr = node.lock(); node_ptr) {
              std::unique_lock lock(me->mutex);
              if (node_ptr.use_count() != 1) {
                // if use_count == 1, this means that the promise has already
                // been scheduled and the node has been removed from the queue
                // otherwise the iterator must still be valid!
                me->removeNode(queue_iterator);
                lock.unlock();
                std::move(node_ptr->promise)
                  .Set(std::make_exception_ptr(::sdb::basics::Exception(
                    cancelled ? ERROR_REQUEST_CANCELED : ERROR_LOCK_TIMEOUT,
                    std::source_location::current())));
              }
            }
          }
        },
        timeout);
    }

    Scheduler& scheduler;
    absl::Mutex mutex;
    // the list is ordered back to front, i.e., new items are added at the
    // front; the last item (back) is the next to be scheduled
    std::list<std::shared_ptr<Node>> queue;
    bool exclusive{false};
    uint32_t lock_count{0};
  };
  std::shared_ptr<SharedState> _shared_state;
};

}  // namespace sdb
