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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <string>
#include <string_view>

#include "basics/common.h"
#include "basics/string_utils.h"

namespace sdb::basics {

class HybridLogicalClock {
 public:
  HybridLogicalClock() : _offset1970{computeOffset1970()} {}

  HybridLogicalClock(const HybridLogicalClock& other) = delete;
  HybridLogicalClock(HybridLogicalClock&& other) = delete;
  HybridLogicalClock& operator=(const HybridLogicalClock& other) = delete;
  HybridLogicalClock& operator=(HybridLogicalClock&& other) = delete;

  TEST_VIRTUAL ~HybridLogicalClock() = default;

  uint64_t getTimeStamp() {
    uint64_t old_time_stamp;
    uint64_t new_time_stamp;
    do {
      uint64_t physical = getPhysicalTime();
      old_time_stamp = _last_time_stamp.load(std::memory_order_relaxed);
      uint64_t old_time = extractTime(old_time_stamp);
      new_time_stamp =
        (physical <= old_time)
          ? assembleTimeStamp(old_time, extractCount(old_time_stamp) + 1)
          : assembleTimeStamp(physical, 0);
    } while (!_last_time_stamp.compare_exchange_weak(
      old_time_stamp, new_time_stamp, std::memory_order_release,
      std::memory_order_relaxed));
    return new_time_stamp;
  }

  // Call the following when a message with a time stamp has been received:
  uint64_t getTimeStamp(uint64_t received_time_stamp) {
    uint64_t old_time_stamp;
    uint64_t new_time_stamp;
    do {
      uint64_t physical = getPhysicalTime();
      old_time_stamp = _last_time_stamp.load(std::memory_order_relaxed);
      uint64_t old_time = extractTime(old_time_stamp);
      uint64_t rec_time = extractTime(received_time_stamp);
      uint64_t new_time = std::max(std::max(old_time, physical), rec_time);
      // Note that this implies newTime >= oldTime and newTime >= recTime
      uint64_t new_count;
      if (new_time == old_time) {
        if (new_time == rec_time) {
          // all three identical
          new_count = std::max(extractCount(old_time_stamp),
                               extractCount(received_time_stamp)) +
                      1;
        } else {
          // this means recTime < newTime
          new_count = extractCount(old_time_stamp) + 1;
        }
      } else {
        // newTime > oldTime
        if (new_time == rec_time) {
          new_count = extractCount(received_time_stamp) + 1;
        } else {
          new_count = 0;
        }
      }
      new_time_stamp = assembleTimeStamp(new_time, new_count);
    } while (!_last_time_stamp.compare_exchange_weak(
      old_time_stamp, new_time_stamp, std::memory_order_release,
      std::memory_order_relaxed));
    return new_time_stamp;
  }

  static std::string_view encodeTimeStamp(uint64_t t, char* r) {
    size_t pos = basics::kMaxU64B64StringSize;
    while (t > 0) {
      SDB_ASSERT(pos > 0);
      r[--pos] = gEncodeTable[t & 0x3f];
      t >>= 6;
    }
    return {r + pos, basics::kMaxU64B64StringSize - pos};
  }

  static auto encodeTimeStamp(uint64_t t) {
    char buffer[basics::kMaxU64B64StringSize];
    return std::string{encodeTimeStamp(t, buffer)};
  }

  static uint64_t decodeTimeStamp(std::string_view s) {
    if (s.size() > 11) {
      return std::numeric_limits<uint64_t>::max();
    }
    uint64_t r = 0;
    for (size_t i = 0; i < s.size(); i++) {
      auto c = gDecodeTable[static_cast<uint8_t>(s[i])];
      if (c < 0) {
        return std::numeric_limits<uint64_t>::max();
      }
      r = (r << 6) | static_cast<uint8_t>(c);
    }
    return r;
  }

  static uint64_t extractTime(uint64_t t) { return t >> 10; }

  static uint64_t extractCount(uint64_t t) { return t & 0x3ff; }

  static uint64_t assembleTimeStamp(uint64_t time, uint64_t count) {
    return (time << 10) + count;
  }

 protected:
  // helper to get the physical time in milliseconds since the epoch:
  TEST_VIRTUAL uint64_t getPhysicalTime() {
    auto now = _clock.now();
    uint64_t ms = std::chrono::duration_cast<std::chrono::microseconds>(
                    now.time_since_epoch())
                    .count() -
                  _offset1970;
    return ms;
  }

  // helper to compute the offset between epoch and 1970
  uint64_t computeOffset1970();

 private:
  using ClockT = std::chrono::high_resolution_clock;
  ClockT _clock;

  std::atomic<uint64_t> _last_time_stamp{0};
  uint64_t _offset1970;

  static const char* gEncodeTable;
  static int8_t gDecodeTable[256];
};

}  // namespace sdb::basics
