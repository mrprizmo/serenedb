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
#include <vector>

#include "basics/application-exit.h"
#include "basics/common.h"
#include "basics/debugging.h"
#include "basics/logger/logger.h"

namespace sdb {

template<typename T>
class FixedSizeAllocator {
 private:
  // sizeof(T) is always a multiple of alignof(T) unless T is packed (which
  // should never be the case here)!
  static_assert((sizeof(T) % alignof(T)) == 0);

  class MemoryBlock {
   public:
    MemoryBlock(const MemoryBlock&) = delete;
    MemoryBlock& operator=(const MemoryBlock&) = delete;

    MemoryBlock(size_t num_items, T* data) noexcept
      : _num_allocated(num_items), _num_used(0), _data(data), _next() {
      SDB_ASSERT(num_items >= 32);

      // the result should at least be 8-byte aligned
      SDB_ASSERT(reinterpret_cast<uintptr_t>(_data) % sizeof(void*) == 0);
      SDB_ASSERT(reinterpret_cast<uintptr_t>(_data) % 64u == 0);
    }

    ~MemoryBlock() noexcept { clear(); }

    MemoryBlock* getNextBlock() const noexcept { return _next; }

    void setNextBlock(MemoryBlock* next) noexcept { _next = next; }

    // return memory address for next in-place object construction.
    T* nextSlot() noexcept {
      SDB_ASSERT(_num_used < _num_allocated);
      return _data + _num_used++;
    }

    bool full() const noexcept { return _num_used == _num_allocated; }

    size_t numUsed() const noexcept { return _num_used; }

    void clear() noexcept {
      // destroy all items
      for (size_t i = 0; i < _num_used; ++i) {
        T* p = _data + i;
        // call destructor for each item
        p->~T();
      }
      _num_used = 0;
      SDB_ASSERT(!full());
    }

   private:
    size_t _num_allocated;
    size_t _num_used;
    T* const _data;
    MemoryBlock* _next;
  };

 public:
  FixedSizeAllocator(const FixedSizeAllocator&) = delete;
  FixedSizeAllocator& operator=(const FixedSizeAllocator&) = delete;

  FixedSizeAllocator() = default;
  ~FixedSizeAllocator() noexcept { clear(); }

  void ensureCapacity() {
    if (_head == nullptr || _head->full()) {
      allocateBlock();
    }
  }

  // requires: ensureCapacity() has been called before!
  template<typename... Args>
  T* allocate(Args&&... args) {
    SDB_ASSERT(_head != nullptr);
    SDB_ASSERT(!_head->full());

    // get memory location for next T object.
    // this moves forward the memory pointer in the memory block
    // and increases _num_used.
    T* p = _head->nextSlot();
    SDB_ASSERT(p != nullptr);
    try {
      // create new T object in place
      new (p) T(std::forward<Args>(args)...);
      return p;
    } catch (...) {
      // if creating the T object has failed, we will have a
      // gap in the memory block with no valid object in it.
      // we cannot easily rollback the memory pointer now, because
      // it may have been moved forward already (e.g. if the ctor
      // of T invoked the same FixedSizeAllocator recursively).
      // thus all we can do here is to patch the "hole" in the
      // memory block with a default-constructed T.
      // if the default ctor of T throws here, there is not much we
      // can do anymore.
      try {
        new (p) T();
      } catch (...) {
        SDB_FATAL("xxxxx", Logger::FIXME,
                  "unrecoverable out-of-memory error in FixedSizeAllocator. "
                  "terminating process");
      }
      throw;
    }
  }

  // calculate the capacity for a block at the given index
  static constexpr size_t capacityForBlock(size_t block_index) {
    return 64 << std::min<size_t>(6, block_index);
  }

  // clears all blocks but the last one (to avoid later
  // re-allocations)
  void clearMost() noexcept {
    auto* block = _head;
    while (block != nullptr) {
      auto* next = block->getNextBlock();
      if (next == nullptr) {
        // we are at the last block
        block->clear();
        _head = block;
        break;
      }

      block->~MemoryBlock();
      ::operator delete(block);
      block = next;
      _head = next;
    }
  }

  void clear() noexcept {
    auto* block = _head;
    while (block != nullptr) {
      auto* next = block->getNextBlock();
      block->~MemoryBlock();
      ::operator delete(block);
      block = next;
    }
    _head = nullptr;
  }

  // return the total number of used elements, in all blocks
  size_t numUsed() const noexcept {
    size_t used = 0;
    auto* block = _head;
    while (block != nullptr) {
      used += block->numUsed();
      block = block->getNextBlock();
    }
    return used;
  }

#ifdef SDB_GTEST
  size_t usedBlocks() const noexcept {
    size_t used = 0;
    auto* block = _head;
    while (block != nullptr) {
      block = block->getNextBlock();
      ++used;
    }
    return used;
  }
#endif

 private:
  void allocateBlock() {
    // minimum block size is for 64 items
    size_t num_items = capacityForBlock(_num_blocks);

    // assumption is that the size of a cache line is at least 64,
    // so we are allocating 64 bytes in addition
    const auto data_size = sizeof(T) * num_items + 64;
    void* p = ::operator new(sizeof(MemoryBlock) + data_size);

    // adjust memory address to cache line offset (assumed to be 64 bytes)
    auto* data = reinterpret_cast<T*>(
      (reinterpret_cast<uintptr_t>(p) + sizeof(MemoryBlock) + 63u) &
      ~(uintptr_t(63u)));

    // creating a MemoryBlock is noexcept, it should not fail
    new (p) MemoryBlock(num_items, data);

    MemoryBlock* block = static_cast<MemoryBlock*>(p);
    block->setNextBlock(_head);
    _head = block;
    ++_num_blocks;
  }

  MemoryBlock* _head{nullptr};
  size_t _num_blocks{0};
};

}  // namespace sdb
