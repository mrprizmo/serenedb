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

#include "basics/resource_usage.h"

#include "basics/debugging.h"
#include "basics/errors.h"
#include "basics/exceptions.h"
#include "basics/global_resource_monitor.h"

using namespace sdb;

ResourceMonitor::~ResourceMonitor() {
#ifdef SDB_DEV
  // this assertion is here to ensure that our memory usage tracking works
  // correctly, and everything that we accounted for is actually properly torn
  // down. the assertion will have no effect in production.
  [[maybe_unused]] auto leftover = _current.load(std::memory_order_relaxed);
  SDB_ASSERT(leftover == 0, "leftover: ", leftover);
#endif
}

/// increase memory usage by <value> bytes. may throw!
/// this function may modify up to 3 atomic variables:
/// - the current local memory usage
/// - the peak local memory usage
/// - the global memory usage
/// the order in which we update these atomic variables is not important,
/// as long as everything is eventually consistent.
/// in case this function triggers a "resource limit exceeded" error,
/// the only thing that can have happened is the update of the current local
/// memory usage, which this function will roll back again.
/// as this function only adds and subtracts values from the memory usage
/// counters, it  does not lead to any lost updates due to data races with
/// other threads.
/// the peak memory usage value is updated with a CAS operation, so again
/// there will no be lost updates.
const char* ResourceMonitor::increaseMemoryUsageNT(uint64_t value) noexcept {
  const uint64_t previous =
    _current.fetch_add(value, std::memory_order_relaxed);
  const uint64_t current = previous + value;
  SDB_ASSERT(current >= value);

  // now calculate if the number of chunks used by instance's allocations stays
  // the same after the extra allocation. if yes, it was likely a very small
  // allocation, and we don't bother with updating the global counter for it.
  // not updating the global counter on each (small) allocation is an
  // optimization that saves updating a (potentially highly contended) shared
  // atomic variable on each allocation. only doing this when there are
  // substantial allocations/deallocations simply makes many cases of small
  // allocations and deallocations more efficient. as a consequence, the
  // granularity of allocations tracked in the global counter is `chunkSize`.
  //
  // this idea is based on suggestions from @dothebart and @mpoeter for reducing
  // the number of updates to the shared global memory usage counter, which very
  // likely would be a source of contention in case multiple queries execute in
  // parallel.
  const int64_t previous_chunks = numChunks(previous);
  const int64_t current_chunks = numChunks(current);
  SDB_ASSERT(current_chunks >= previous_chunks);
  auto diff = current_chunks - previous_chunks;

  if (diff != 0) {
    auto rollback = [this, value, diff] noexcept {
      // When rolling back, we have to consider that our change to the local
      // memory usage might have affected other threads. Suppose we have a chunk
      // size of 10 and a global memory limit of 20 (=2 chunks).
      //   - Thread A increments local memory by 18 (=> exact global=18) and
      //   global memory by 1 chunk
      //   - Thread B increments local memory by 13 (=> exact global=31) and
      //   attempts to update global
      //     memory by 2 chunks, which would exceed the limit, but before we can
      //     rollback the update to the local memory, Thread A already decreases
      //     by 18 again (=> exact global=13), so Thread A would decrease global
      //     memory by 2 chunks!
      // Thread A first increases by 1 chunk, but later decreases by 2 chunks -
      // this can cause the global memory to underflow! The reason for this
      // difference is due to the change to local memory by Thread B, so any
      // such difference has to be considered during rollback.
      //
      // When Thread B now performs its rollback - decreasing by 13 (=> exact
      // global=0) - we would decrease global memory by 1 chunk, but our initial
      // attempt to increase was 2 chunks - this is exactly the difference of 1
      // chunk that Thread A has decreased more, so we have to take this into
      // account and increase global memory by 1 chunk to balance this out.
      uint64_t adjusted_previous =
        _current.fetch_sub(value, std::memory_order_relaxed);
      uint64_t adjusted_current = adjusted_previous - value;

      int64_t adjusted_diff =
        diff + (numChunks(adjusted_current) - numChunks(adjusted_previous));
      if (adjusted_diff != 0) {
        SDB_ASSERT(adjusted_diff == 1 || adjusted_diff == -1);
        // adjustment can be off by at most 1.
        // forceUpdateMemoryUsage takes a signed int64, so we can
        // increase/decrease
        _global.forceUpdateMemoryUsage(adjusted_diff * kChunkSize);
      }
    };

    // number of chunks has changed, so this is either a substantial allocation
    // or we have piled up changes by lots of small allocations so far. time for
    // some memory expensive checks now...

    if (_limit > 0 && SDB_UNLIKELY(current > _limit)) {
      // we would use more memory than dictated by the instance's own limit.
      // because we will throw an exception directly afterwards, we now need to
      // revert the change that we already made to the instance's own counter.
      rollback();

      // track local limit violation
      _global.trackLocalViolation();
      // now we can safely signal an exception
      return "query would use more memory than allowed";
    }

    // instance's own memory usage counter has been updated successfully once we
    // got here.

    // now modify the global counter value, too.
    if (!_global.increaseMemoryUsage(diff * kChunkSize)) {
      // the allocation would exceed the global maximum value, so we need to
      // roll back.
      rollback();

      // track global limit violation
      _global.trackGlobalViolation();
      // now we can safely signal an exception
      return "global memory limit exceeded";
    }
    // increasing the global counter has succeeded!

    // update the peak memory usage counter for the local instance. we do this
    // only when there was a change in the number of chunks.
    uint64_t peak = _peak.load(std::memory_order_relaxed);
    const uint64_t new_peak = current_chunks * kChunkSize;
    // do a CAS here, as concurrent threads may work on the peak memory usage at
    // the very same time.
    while (peak < new_peak) {
      if (_peak.compare_exchange_weak(peak, new_peak, std::memory_order_release,
                                      std::memory_order_relaxed)) {
        break;
      }
    }
  }
  return nullptr;
}

void ResourceMonitor::increaseMemoryUsage(uint64_t value) {
  if (const char* error = increaseMemoryUsageNT(value)) {
    SDB_THROW(ERROR_RESOURCE_LIMIT, error);
  }
}

void ResourceMonitor::decreaseMemoryUsage(uint64_t value) noexcept {
  // this function will always decrease the current local memory usage value,
  // and may in addition lead to a decrease of the global memory usage value.
  // as it only subtracts from these counters, it will not lead to any lost
  // updates even if there are concurrent threads working on the same counters.
  // note that peak memory usage is not changed here, as this is only relevant
  // when we are _increasing_ memory usage.
  const uint64_t previous =
    _current.fetch_sub(value, std::memory_order_relaxed);
  SDB_ASSERT(previous >= value);
  const uint64_t current = previous - value;

  const int64_t diff = numChunks(previous) - numChunks(current);

  if (diff != 0) {
    // number of chunks has changed. now we will update the global counter!
    _global.decreaseMemoryUsage(diff * kChunkSize);
    // no need to update the peak memory usage counter here
  }
}

uint64_t ResourceMonitor::current() const noexcept {
  return _current.load(std::memory_order_relaxed);
}

uint64_t ResourceMonitor::peak() const noexcept {
  return _peak.load(std::memory_order_relaxed);
}
