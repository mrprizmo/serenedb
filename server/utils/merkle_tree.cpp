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

#include "merkle_tree.h"

#include "basics/debugging.h"
#include "basics/encoding_utils.h"
#include "basics/endian.h"
#include "basics/hashes.h"
#include "basics/hybrid_logical_clock.h"
#include "basics/number_utils.h"
#include "basics/static_strings.h"
#include "basics/string_utils.h"

#ifdef SDB_FAULT_INJECTION
#include "basics/random/random_generator.h"
#endif

#include <vpack/builder.h>
#include <vpack/iterator.h>
#include <vpack/slice.h>

#define LZ4_STATIC_LINKING_ONLY
#include <absl/cleanup/cleanup.h>
#include <lz4.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// can turn this on for more aggressive and expensive consistency checks
// #define PARANOID_TREE_CHECKS

namespace sdb::containers {
namespace {

constexpr uint8_t kCurrentVersion = 0x01;

template<typename T>
T ReadUInt(const char*& p) noexcept {
  T value;
  memcpy(&value, p, sizeof(T));
  p += sizeof(T);
  return sdb::basics::LittleToHost<T>(value);
};

}  // namespace

uint64_t FnvHashProvider::hash(uint64_t input) const {
  return FnvHashPod(input);
}

static_assert(std::is_trivial_v<MerkleTreeBase::Meta> &&
              std::is_standard_layout_v<MerkleTreeBase::Meta>);

void MerkleTreeBase::Data::ensureShard(uint64_t shard, uint64_t shard_size) {
  if (shards.size() <= shard) {
    if (shards.capacity() <= shard) {
      shards.resize(std::max(shards.capacity() * 2, shard + 1));
    } else {
      shards.resize(shard + 1);
    }
  }
  if (!shards[shard]) {
    shards[shard] = buildShard(shard_size);
    SDB_ASSERT(shards[shard]);
    memory_usage += shard_size;
  }
}

MerkleTreeBase::Data::ShardType MerkleTreeBase::Data::buildShard(
  uint64_t shard_size) {
  static_assert(std::is_trivially_constructible_v<Node>);
  static_assert(std::is_trivially_copyable_v<Node>);
  // note: shardSize is currently passed in bytes, not in number of nodes!
  SDB_ASSERT(shard_size % kNodeSize == 0);
  auto p = std::make_unique<Node[]>(shard_size / kNodeSize);
  memset(p.get(), 0, shard_size);
  return p;
}

void MerkleTreeBase::Node::toVPack(vpack::Builder& output) const {
  vpack::ObjectBuilder node_guard(&output);
  output.add(StaticStrings::kRevisionTreeCount, count);
  output.add(StaticStrings::kRevisionTreeHash, hash);
}

bool MerkleTreeBase::Node::operator==(const Node& other) const noexcept {
  return (this->count == other.count) && (this->hash == other.hash);
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::defaultRange(uint64_t depth) {
  // start with 64 revisions per leaf; this is arbitrary, but the key is we want
  // to start with a relatively fine-grained tree so we can differentiate well,
  // but we don't want to go so small that we have to resize immediately
  return nodeCountAtDepth(depth) * 64;
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::numberOfShards() const noexcept {
  SDB_ASSERT(allocationSize(meta().depth) - kMetaSize > 0);
  // we must have at least one shard
  return std::max(uint64_t(1),
                  ((allocationSize(meta().depth) - kMetaSize) / kShardSize));
}

template<typename Hasher, const uint64_t BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromBuffer(std::string_view buffer) {
  if (buffer.size() <
      kMetaSize - sizeof(Meta::Padding) + 2 /*for versioning*/) {
    throw std::invalid_argument("Input buffer is too small");
  }
  SDB_ASSERT(buffer.size() > 2);

  auto version = static_cast<uint8_t>(buffer[buffer.size() - 1]);
  if (version != kCurrentVersion) {
    throw std::invalid_argument(
      "Buffer does not contain a properly versioned tree");
  }

  auto format = static_cast<BinaryFormat>(buffer[buffer.size() - 2]);
  buffer.remove_suffix(2);

  if (format == BinaryFormat::Uncompressed) {
    return fromUncompressed(buffer);
  } else if (format == BinaryFormat::LZ4) {
    return fromLZ4(buffer);
  } else if (format == BinaryFormat::Testing) {
    return fromTesting(buffer);
  }
  throw std::invalid_argument("unknown tree serialization type");
}

template<typename Hasher, const uint64_t BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromTesting(std::string_view buffer) {
  if (buffer.size() < kMetaSize) {
    // not enough space to even store the meta info, can't proceed
    return nullptr;
  }

  const Meta& meta = *reinterpret_cast<const Meta*>(buffer.data());
  if (buffer.size() != kMetaSize + (kNodeSize * nodeCountAtDepth(meta.depth))) {
    // allocation size doesn't match meta data, can't proceed
    return nullptr;
  }

  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
    new MerkleTree<Hasher, BranchingBits>(buffer));
}

template<typename Hasher, const uint64_t BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromLZ4(std::string_view buffer) {
  const char* p = buffer.data();
  const char* e = p + buffer.size();

  if (p + kMetaSize - sizeof(Meta::Padding) + sizeof(uint32_t) >= e) {
    throw std::invalid_argument(
      "invalid compressed tree data in buffer (lazy compression)");
  }

  Data data;

  data.meta.range_min = ReadUInt<uint64_t>(p);
  data.meta.range_max = ReadUInt<uint64_t>(p);
  data.meta.depth = ReadUInt<uint64_t>(p);
  data.meta.initial_range_min = ReadUInt<uint64_t>(p);

  uint64_t summary_count = ReadUInt<uint64_t>(p);
  uint64_t summary_hash = ReadUInt<uint64_t>(p);
  data.meta.summary = {summary_count, summary_hash};

  SDB_ASSERT(p - buffer.data() == kMetaSize - sizeof(Meta::Padding));

  uint32_t number_of_shards = ReadUInt<uint32_t>(p);
  if (number_of_shards >= 1024) {
    throw std::invalid_argument("invalid compressed shard count in tree data");
  }
  // initialize all shards to nullptrs
  data.shards.resize(number_of_shards);

  const char* compressed_data = p + number_of_shards * sizeof(uint32_t);
  for (uint64_t i = 0; i < number_of_shards; ++i) {
    SDB_ASSERT(p < e);
    uint32_t compressed_length = ReadUInt<uint32_t>(p);
    if (compressed_length != 0) {
      int shard_length = shardSize(data.meta.depth);
      SDB_ASSERT(shard_length % kNodeSize == 0);
      // ShardSize may be more than we actually need (if we only have a single
      // shard), but it doesn't matter for correctness here.
      data.ensureShard(i, shard_length);

      int decompressed_length = LZ4_decompress_safe(
        compressed_data, reinterpret_cast<char*>(data.shards[i].get()),
        compressed_length, shard_length);
      if (decompressed_length != shard_length) {
        throw std::invalid_argument("Cannot uncompress LZ4-compressed data.");
      }
      compressed_data += compressed_length;
    }
  }
  SDB_ASSERT(compressed_data == e);

  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
    new MerkleTree<Hasher, BranchingBits>(std::move(data)));
}

template<typename Hasher, const uint64_t BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::fromUncompressed(std::string_view buffer) {
  const char* p = buffer.data();
  const char* e = p + buffer.size();

  if (p + kMetaSize - sizeof(Meta::Padding) > e) {
    throw std::invalid_argument(
      "invalid compressed tree data in bottommost buffer");
  }

  uint64_t range_min = ReadUInt<uint64_t>(p);
  uint64_t range_max = ReadUInt<uint64_t>(p);
  uint64_t depth = ReadUInt<uint64_t>(p);
  uint64_t initial_range_min = ReadUInt<uint64_t>(p);
  uint64_t summary_count = ReadUInt<uint64_t>(p);
  uint64_t summary_hash = ReadUInt<uint64_t>(p);

  auto tree = std::make_unique<MerkleTree<Hasher, BranchingBits>>(
    depth, range_min, range_max, initial_range_min);

  uint64_t total_count = 0;
  uint64_t total_hash = 0;

  while (p + sizeof(uint32_t) + sizeof(uint64_t) + sizeof(uint64_t) <= e) {
    // enough data to read
    uint32_t pos = ReadUInt<uint32_t>(p);
    uint64_t count = ReadUInt<uint64_t>(p);
    uint64_t hash = ReadUInt<uint64_t>(p);

    if (count != 0 || hash != 0) {
      tree->node(pos) = {count, hash};

      total_count += count;
      total_hash ^= hash;
    }
  }

  if (p != e) {
    throw std::invalid_argument(
      "invalid compressed tree data with overflow values");
  }

  if (summary_count != total_count || summary_hash != total_hash) {
    throw std::invalid_argument("invalid compressed tree summary data");
  }

  // write summary node
  Node& summary = tree->meta().summary;
  summary = {total_count, total_hash};

  return tree;
}

template<typename Hasher, const uint64_t BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::deserialize(vpack::Slice slice) {
  if (!slice.isObject()) {
    return nullptr;
  }

  vpack::Slice read = slice.get(StaticStrings::kRevisionTreeVersion);
  if (!read.isNumber() || read.getNumber<uint8_t>() != kCurrentVersion) {
    return nullptr;
  }

  read = slice.get(StaticStrings::kRevisionTreeMaxDepth);
  if (!read.isNumber()) {
    return nullptr;
  }
  uint64_t depth = read.getNumber<uint64_t>();

  read = slice.get(StaticStrings::kRevisionTreeRangeMax);
  if (!read.isString()) {
    return nullptr;
  }
  auto range_max =
    basics::HybridLogicalClock::decodeTimeStamp(read.stringView());
  if (range_max == std::numeric_limits<uint64_t>::max()) {
    return nullptr;
  }

  read = slice.get(StaticStrings::kRevisionTreeRangeMin);
  if (!read.isString()) {
    return nullptr;
  }
  auto range_min =
    basics::HybridLogicalClock::decodeTimeStamp(read.stringView());
  if (range_min == std::numeric_limits<uint64_t>::max()) {
    return nullptr;
  }

  read = slice.get(StaticStrings::kRevisionTreeInitialRangeMin);
  uint64_t initial_range_min =
    read.isString()
      ? basics::HybridLogicalClock::decodeTimeStamp(read.stringView())
      : std::numeric_limits<uint64_t>::max();
  if (initial_range_min == std::numeric_limits<uint64_t>::max()) {
    return nullptr;
  }

  // summary count
  read = slice.get(StaticStrings::kRevisionTreeCount);
  if (!read.isNumber()) {
    return nullptr;
  }
  uint64_t summary_count = read.getNumber<uint64_t>();

  // summary hash
  read = slice.get(StaticStrings::kRevisionTreeHash);
  if (!read.isNumber()) {
    return nullptr;
  }
  uint64_t summary_hash = read.getNumber<uint64_t>();

  vpack::Slice nodes = slice.get(StaticStrings::kRevisionTreeNodes);
  if (!nodes.isArray() || nodes.length() > nodeCountAtDepth(depth)) {
    // note: we can have less nodes than nodeCountAtDepth because of
    // "skip" nodes
    return nullptr;
  }

  // allocate the tree
  SDB_ASSERT(range_min < range_max);
  auto tree = std::make_unique<MerkleTree<Hasher, BranchingBits>>(
    depth, range_min, range_max, initial_range_min);

  uint64_t total_count = 0;
  uint64_t total_hash = 0;
  uint64_t index = 0;
  for (vpack::Slice node_slice : vpack::ArrayIterator(nodes)) {
    SDB_ASSERT(node_slice.isObject());

    vpack::Slice skip_slice = node_slice.get("skip");
    if (!skip_slice.isNone()) {
      index += skip_slice.getNumber<uint64_t>();
      continue;
    }

    vpack::Slice count_slice =
      node_slice.get(StaticStrings::kRevisionTreeCount);
    vpack::Slice hash_slice = node_slice.get(StaticStrings::kRevisionTreeHash);

    // skip any nodes for which there is neither a count nor a hash value
    // present. such nodes will be returned by versions which support the
    // "onlyPopulated" URL parameter for improved efficiency.
    if (!count_slice.isNone() || !hash_slice.isNone()) {
      // if any attribute is present, both need to be numeric!
      if (!count_slice.isNumber() || !hash_slice.isNumber()) {
        return nullptr;
      }
      uint64_t count = count_slice.getNumber<uint64_t>();
      uint64_t hash = hash_slice.getNumber<uint64_t>();
      if (count != 0 || hash != 0) {
        tree->node(index) = {count, hash};

        total_count += count;
        total_hash ^= hash;
      }
    }

    ++index;
  }

  if (total_count != summary_count || total_hash != summary_hash) {
    return nullptr;
  }

  tree->meta().summary = {total_count, total_hash};

  return tree;
}

template<typename Hasher, const uint64_t BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(uint64_t depth,
                                              uint64_t range_min,
                                              uint64_t range_max,
                                              uint64_t initial_range_min) {
  if (depth < 2) {
    throw std::invalid_argument("Must specify a depth >= 2");
  }

  SDB_ASSERT(range_max == 0 || range_max > range_min);
  if (initial_range_min == 0) {
    initial_range_min = range_min;
  }
  SDB_ASSERT(range_min <= initial_range_min);

  if (range_max == 0) {
    // default value for rangeMax is 0
    range_max = range_min + defaultRange(depth);
    SDB_ASSERT(range_min < range_max);
  }

  if (range_max <= range_min) {
    throw std::invalid_argument("rangeMax must be larger than rangeMin");
  }
  if (!number_utils::IsPowerOf2(range_max - range_min)) {
    throw std::invalid_argument(
      "Expecting difference between min and max to be power of 2");
  }
  if (range_max - range_min < nodeCountAtDepth(depth)) {
    throw std::invalid_argument(
      "Need at least one revision in each bucket in deepest layer");
  }
  SDB_ASSERT(nodeCountAtDepth(depth) > 0);
  SDB_ASSERT(range_max - range_min != 0);

  if ((initial_range_min - range_min) %
        ((range_max - range_min) / nodeCountAtDepth(depth)) !=
      0) {
    throw std::invalid_argument(
      "Expecting difference between initial min and min to be divisible by "
      "(max-min)/nodeCountAt(depth)");
  }

  SDB_ASSERT(((range_max - range_min) / nodeCountAtDepth(depth)) *
               nodeCountAtDepth(depth) ==
             (range_max - range_min));

  // no lock necessary here
  _data.meta = Meta{range_min,
                    range_max,
                    depth,
                    initial_range_min,
                    /*summary node*/ {0, 0},
                    /*padding*/ {0, 0}};

  SDB_ASSERT(this->meta().summary.count == 0);
  SDB_ASSERT(this->meta().summary.hash == 0);
}

template<typename Hasher, const uint64_t BranchingBits>
MerkleTree<Hasher, BranchingBits>::~MerkleTree() = default;

template<typename Hasher, const uint64_t BranchingBits>
MerkleTree<Hasher, BranchingBits>& MerkleTree<Hasher, BranchingBits>::operator=(
  std::unique_ptr<MerkleTree<Hasher, BranchingBits>>&& other) {
  if (!other) {
    return *this;
  }

  std::scoped_lock guard1(_data_lock, other->_data_lock);

  SDB_ASSERT(this->meta().depth == other->meta().depth);

  _data = std::move(other->_data);

  return *this;
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::memoryUsage() const {
  absl::ReaderMutexLock guard{&_data_lock};
  return kMetaSize + _data.memory_usage;
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::dynamicMemoryUsage() const {
  absl::ReaderMutexLock guard{&_data_lock};
  return _data.memory_usage;
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::count() const {
  absl::ReaderMutexLock guard{&_data_lock};
  return meta().summary.count;
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::rootValue() const {
  absl::ReaderMutexLock guard{&_data_lock};
  return meta().summary.hash;
}

template<typename Hasher, const uint64_t BranchingBits>
std::pair<uint64_t, uint64_t> MerkleTree<Hasher, BranchingBits>::range() const {
  absl::ReaderMutexLock guard{&_data_lock};
  return {meta().range_min, meta().range_max};
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::depth() const {
  absl::ReaderMutexLock guard{&_data_lock};
  return meta().depth;
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::byteSize() const {
  absl::ReaderMutexLock guard{&_data_lock};
  return allocationSize(meta().depth);
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::prepareInsertMinMax(
  std::unique_lock<absl::Mutex>& guard, uint64_t min_key, uint64_t max_key) {
  if (min_key < meta().range_min) {
    growLeft(min_key);
  }

  if (max_key >= meta().range_max) {
    growRight(max_key);
  }
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::insert(uint64_t key) {
  std::unique_lock guard(_data_lock);

  // may grow the tree so it can store key
  prepareInsertMinMax(guard, key, key);

  modify(key, /*isInsert*/ true);
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::insert(
  const std::vector<uint64_t>& keys) {
  if (keys.size() == 1) {
    // optimization for a single key
    return this->insert(keys[0]);
  }

  if (keys.empty()) [[unlikely]] {
    return;
  }

  std::vector<uint64_t> sorted_keys = keys;
  // we need to sort here so that we can easily determine the
  // minimum and maximum key values. we need these values to do
  // the bounds-check on the tree later.
  std::sort(sorted_keys.begin(), sorted_keys.end());

  uint64_t min_key = sorted_keys.front();
  uint64_t max_key = sorted_keys.back();

  std::unique_lock guard(_data_lock);

  // may grow the tree so it can store minKey and MaxKey
  prepareInsertMinMax(guard, min_key, max_key);

  modify(sorted_keys, /*isInsert*/ true);
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::remove(uint64_t key) {
  std::unique_lock guard(_data_lock);

  if (key < meta().range_min || key >= meta().range_max) {
    throw std::out_of_range("Cannot remove, key out of current range.");
  }

  modify(key, /*isInsert*/ false);
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::remove(
  const std::vector<uint64_t>& keys) {
  if (keys.size() == 1) {
    // optimization for a single key
    return remove(keys[0]);
  }

  if (keys.empty()) [[unlikely]] {
    return;
  }

  std::vector<uint64_t> sorted_keys = keys;
  // we need to sort here so that we can easily determine the
  // minimum and maximum key values. we need these values to do
  // the bounds-check on the tree later.
  std::sort(sorted_keys.begin(), sorted_keys.end());

  uint64_t min_key = sorted_keys.front();
  uint64_t max_key = sorted_keys.back();

  std::unique_lock guard(_data_lock);

  // we don't grow the tree automatically here, as remove operations
  // are only expected for keys that have been added before.
  if (min_key < meta().range_min || max_key >= meta().range_max) {
    throw std::out_of_range("Cannot remove, key out of current range.");
  }

  modify(sorted_keys, /*isInsert*/ false);
}

#ifdef SDB_FAULT_INJECTION
template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::removeUnsorted(
  const std::vector<uint64_t>& keys) {
  if (keys.size() < 2) {
    return remove(keys);
  }

  std::unique_lock guard(_data_lock);

  modify(keys, /*isInsert*/ false);
}
#endif

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::clear() {
  std::unique_lock guard(_data_lock);

  _data.clear();
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::checkConsistency() const {
  absl::ReaderMutexLock guard{&_data_lock};

  checkInternalConsistency();
}

#ifdef SDB_FAULT_INJECTION
// intentionally corrupts the tree. used for testing only
template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::corrupt(uint64_t count, uint64_t hash) {
  std::unique_lock guard(_data_lock);

  meta().summary = {count, hash};

  // we also need to corrupt the lowest level, simply because in case the
  // bottom-most level format is used, we will lose all corruption on upper
  // levels

  for (uint64_t i = 0; i < 4; ++i) {
    auto pos = sdb::random::Interval(
      static_cast<uint32_t>(nodeCountAtDepth(meta().depth)));

    Node& node = this->node(pos);
    node.count = sdb::random::Interval(UINT32_MAX);
    node.hash = sdb::random::Interval(UINT32_MAX);
  }
}
#endif

template<typename Hasher, const uint64_t BranchingBits>
std::unique_ptr<MerkleTree<Hasher, BranchingBits>>
MerkleTree<Hasher, BranchingBits>::clone() {
  // acquire the read-lock here to protect ourselves from concurrent
  // modifications
  absl::ReaderMutexLock guard{&_data_lock};

  // cannot use make_unique here as the called ctor is protected
  return std::unique_ptr<MerkleTree<Hasher, BranchingBits>>(
    new MerkleTree<Hasher, BranchingBits>(*this));
}

template<typename Hasher, const uint64_t BranchingBits>
std::vector<std::pair<uint64_t, uint64_t>>
MerkleTree<Hasher, BranchingBits>::diff(
  MerkleTree<Hasher, BranchingBits>& other) {
  std::scoped_lock guard(_data_lock, other._data_lock);

  uint64_t depth = this->meta().depth;
  if (depth != other.meta().depth) {
    throw std::invalid_argument("Expecting two trees with same depth.");
  }

  while (true) {  // left by break
    uint64_t width = this->meta().range_max - this->meta().range_min;
    uint64_t width_other = other.meta().range_max - other.meta().range_min;
    if (width == width_other) {
      break;
    }
    if (width < width_other) {
      // grow this times 2:
      uint64_t range_max = this->meta().range_max;
      this->growRight(range_max);
    } else {
      // grow other times 2:
      uint64_t range_max = other.meta().range_max;
      other.growRight(range_max);
    }
    // loop to repeat, this also helps to make sure someone else didn't
    // grow while we switched between shared/exclusive locks
  }
  // Now both trees have the same width, but they might still have a
  // different rangeMin. However, by invariant 2 we know that their
  // difference is divisible by the number keys in a bucket in the
  // bottommost level. Therefore, we can adjust the rest by shifting
  // by a multiple of a bucket.
  auto tree1 = this;
  auto tree2 = &other;
  if (tree2->meta().range_min < tree1->meta().range_min) {
    // swap trees so that tree1 always has an equal or lower rangeMin than
    // tree2.
    auto dummy = tree1;
    tree1 = tree2;
    tree2 = dummy;
  }
  // Now the rangeMin of tree1 is <= the rangeMin of tree2.
  SDB_ASSERT(tree1->meta().range_min <= tree2->meta().range_min);

  SDB_ASSERT(tree1->numberOfShards() == tree2->numberOfShards());

  std::vector<std::pair<uint64_t, uint64_t>> result;
  uint64_t n = nodeCountAtDepth(depth);

  auto add_range = [&result](uint64_t min, uint64_t max) {
    if (!result.empty() && result.back().second + 1 == min) {
      // Extend range: //
      result.back().second = max;
    } else {
      result.push_back(std::make_pair(min, max));
    }
  };

  // First do the stuff tree2 does not even have:
  uint64_t keys_per_bucket =
    (tree1->meta().range_max - tree1->meta().range_min) / n;
  uint64_t index1 = 0;
  uint64_t pos = tree1->meta().range_min;
  for (; pos < tree2->meta().range_min && pos < tree1->meta().range_max;
       pos += keys_per_bucket) {
    const Node& node1 = tree1->node(index1);
    if (node1.count != 0) {
      add_range(pos, pos + keys_per_bucket - 1);
    }
    ++index1;
  }
  // Now the buckets they both have:
  SDB_ASSERT(pos == tree2->meta().range_min ||
             (pos == tree1->meta().range_max &&
              tree2->meta().range_min > tree1->meta().range_max));
  // note that pos can be < tree2->meta().rangeMin if the trees do not overlap
  // at all
  uint64_t index2 = 0;
  for (pos = tree2->meta().range_min; pos < tree1->meta().range_max;
       pos += keys_per_bucket) {
    const Node& node1 = tree1->node(index1);
    const Node& node2 = tree2->node(index2);
    if (node1.hash != node2.hash || node1.count != node2.count) {
      add_range(pos, pos + keys_per_bucket - 1);
    }
    ++index1;
    ++index2;
  }
  // And finally the rest of tree2:
  for (; pos < tree2->meta().range_max; pos += keys_per_bucket) {
    const Node& node2 = tree2->node(index2);
    if (node2.count != 0) {
      add_range(pos, pos + keys_per_bucket - 1);
    }
    ++index2;
  }

  return result;
}

template<typename Hasher, const uint64_t BranchingBits>
std::string MerkleTree<Hasher, BranchingBits>::toString(bool full) const {
  std::string output;

  absl::ReaderMutexLock guard{&_data_lock};

  if (full) {
    output.append("Merkle-tree ");
    output.append("- depth: ");
    output.append(std::to_string(meta().depth));
    output.append(", rangeMin: ");
    output.append(std::to_string(meta().range_min));
    output.append(", rangeMax: ");
    output.append(std::to_string(meta().range_max));
    output.append(", initialRangeMin: ");
    output.append(std::to_string(meta().initial_range_min));
    output.append(", count: ");
    output.append(std::to_string(meta().summary.count));
    output.append(", hash: ");
    output.append(std::to_string(meta().summary.hash));
    output.append(" ");
  }
  output.append("[");

  uint64_t last = nodeCountAtDepth(meta().depth);
  for (uint64_t chunk = 0; chunk < last; ++chunk) {
    const Node& node = this->node(chunk);
    output.append("[");
    output.append(std::to_string(node.count));
    output.append(",");
    output.append(std::to_string(node.hash));
    output.append("],");
  }
  output.append("]");
  return output;
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serialize(vpack::Builder& output,
                                                  bool only_populated) const {
  // only a minimum allocation here. we may need a lot more
  output.reserve(only_populated ? 1024 : 256 * 1024);
  char rev_buf[basics::kMaxU64B64StringSize];

  absl::ReaderMutexLock guard{&_data_lock};

  uint64_t depth = meta().depth;

  vpack::ObjectBuilder top_level_guard(&output);
  output.add(StaticStrings::kRevisionTreeVersion, kCurrentVersion);
  output.add(StaticStrings::kRevisionTreeMaxDepth, depth);
  output.add(
    StaticStrings::kRevisionTreeRangeMax,
    basics::HybridLogicalClock::encodeTimeStamp(meta().range_max, rev_buf));
  output.add(
    StaticStrings::kRevisionTreeRangeMin,
    basics::HybridLogicalClock::encodeTimeStamp(meta().range_min, rev_buf));
  output.add(StaticStrings::kRevisionTreeInitialRangeMin,
             basics::HybridLogicalClock::encodeTimeStamp(
               meta().initial_range_min, rev_buf));
  output.add(StaticStrings::kRevisionTreeCount, meta().summary.count);
  output.add(StaticStrings::kRevisionTreeHash, meta().summary.hash);

  vpack::ArrayBuilder node_array_guard(&output,
                                       StaticStrings::kRevisionTreeNodes);

  uint64_t to_skip = 0;
  uint64_t last = nodeCountAtDepth(depth);
  for (uint64_t index = 0; index < last; ++index) {
    const Node& src = this->node(index);
    if (only_populated) {
      if (src.empty()) {
        // node is empty. simply count how many empty nodes we have in a run
        ++to_skip;
      } else {
        // check if we have a run of empty nodes...
        if (to_skip == 1) {
          // return an empty object. this is still worse than returning nothing
          // at all, but otherwise we would need to return the index number of
          // each populated node, which is likely more data
          output.add(vpack::Slice::emptyObjectSlice());
        } else if (to_skip > 1) {
          // cheat and only return the number of empty buckets
          output.openObject();
          output.add("skip", to_skip);
          output.close();
        }
        to_skip = 0;
        src.toVPack(output);
      }
    } else {
      src.toVPack(output);
    }
  }
  // if onlyPopulated = true, we intentionally don't serialize any
  // to-skip nodes at the end, because they are optional
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serializeBinary(
  std::string& output, BinaryFormat format) const {
  SDB_ASSERT(output.empty());

  absl::ReaderMutexLock guard{&_data_lock};

  if (format == BinaryFormat::Optimal) {
    // 15'000 is an arbitrary cutoff value
    if (meta().summary.count <= 15'000) {
      format = BinaryFormat::Uncompressed;
    } else {
      format = BinaryFormat::LZ4;
    }
  }

  SDB_IF_FAILURE("MerkleTree::serializeTesting") {
    format = BinaryFormat::Testing;
  }
  SDB_IF_FAILURE("MerkleTree::serializeUncompressed") {
    format = BinaryFormat::Uncompressed;
  }
  SDB_IF_FAILURE("MerkleTree::serializeLZ4") { format = BinaryFormat::LZ4; }

  SDB_ASSERT(format != BinaryFormat::Optimal);

  switch (format) {
    case BinaryFormat::Uncompressed: {
      // make a minimum allocation that will be enough for the meta data and a
      // few nodes. if we need more space, the string can grow as needed
      output.reserve(64);
      serializeMeta(output, /*usePadding*/ false);
      serializeNodes(output, /*all*/ false);
    } break;
    case BinaryFormat::LZ4: {
      output.reserve(4096);
      serializeMeta(output, /*usePadding*/ false);
      const auto number_of_shards = static_cast<uint32_t>(numberOfShards());
      auto value = basics::HostToLittle(number_of_shards);
      output.append(reinterpret_cast<const char*>(&value), sizeof(uint32_t));

      const size_t table_base = output.size();
      SDB_ASSERT(table_base ==
                 kMetaSize - sizeof(Meta::Padding) + sizeof(uint32_t));

      // for each shard, write a placeholder for the compressed length.
      // the placeholders are initially filled with 0 bytes lengths for each
      // shards. these values will be overridden later for the shards that
      // actually contain data. for fully empty shards, the 0 values will remain
      // in place.
      output.append(number_of_shards * sizeof(uint32_t), '\0');

      SDB_ASSERT(output.size() == kMetaSize - sizeof(Meta::Padding) +
                                    sizeof(uint32_t) +
                                    number_of_shards * sizeof(uint32_t));

      auto* stream = encoding::MakeLZ4Stream();
      const auto src_size = shardSize(meta().depth);
      const auto dst_capacity = LZ4_COMPRESSBOUND(src_size);
      for (uint32_t i = 0; i < number_of_shards; ++i) {
        int compressed_size = 0;
        if (i < _data.shards.size() && _data.shards[i]) {
          const char* src_data = reinterpret_cast<char*>(_data.shards[i].get());
          const auto dst_old_size = output.size();
          basics::StrAppend(output, dst_capacity);
          compressed_size = LZ4_compress_fast_extState_fastReset(
            stream, src_data, output.data() + dst_old_size, src_size,
            dst_capacity, 1);
          if (compressed_size <= 0) {
            throw std::invalid_argument("LZ4_compress_fast failed");
          }
          output.erase(dst_old_size + compressed_size);
        }
        // patch offsets table with compressed length
        absl::little_endian::Store32(&output[table_base + i * sizeof(uint32_t)],
                                     compressed_size);
      }
    } break;
    case BinaryFormat::Testing: {
      output.reserve(allocationSize(meta().depth) + 1);
      serializeMeta(output, /*usePadding*/ true);
      serializeNodes(output, /*all*/ true);
    } break;
    default: {
      SDB_ASSERT(false);
      throw std::invalid_argument("Invalid binary format");
    }
  }

  // append format and version
  output.push_back(static_cast<char>(format));
  output.push_back(static_cast<char>(kCurrentVersion));
}

template<typename Hasher, const uint64_t BranchingBits>
std::vector<std::pair<uint64_t, uint64_t>>
MerkleTree<Hasher, BranchingBits>::partitionKeys(uint64_t count) const {
  std::vector<std::pair<uint64_t, uint64_t>> result;

  absl::ReaderMutexLock guard{&_data_lock};
  uint64_t remaining = meta().summary.count;

  if (count <= 1 || remaining == 0) {
    // special cases, just return full range
    result.emplace_back(meta().range_min, meta().range_max);
    return result;
  }

  uint64_t depth = meta().depth;
  uint64_t target_count = std::max(static_cast<uint64_t>(1), remaining / count);
  uint64_t range_start = meta().range_min;
  uint64_t range_count = 0;
  uint64_t last = nodeCountAtDepth(depth);
  for (uint64_t chunk = 0; chunk < last; ++chunk) {
    if (result.size() == count - 1) {
      // if we are generating the last partition, just fast forward to the last
      // chunk, put everything in
      chunk = last - 1;
    }
    uint64_t index = chunk;
    const Node& node = this->node(index);
    range_count += node.count;
    if (range_count >= target_count || chunk == last - 1) {
      auto [_, rangeEnd] = chunkRange(chunk, depth);
      result.emplace_back(range_start, rangeEnd);
      remaining -= range_count;
      if (remaining == 0 || result.size() == count) {
        // if we just finished the last partiion, shortcut out
        break;
      }
      range_count = 0;
      range_start = rangeEnd + 1;
      target_count =
        std::max(static_cast<uint64_t>(1), remaining / (count - result.size()));
    }
  }

  SDB_ASSERT(result.size() <= count);

  return result;
}

template<typename Hasher, const uint64_t BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(std::string_view buffer) {
  if (buffer.size() < allocationSize(/*minDepth*/ 2)) {
    throw std::invalid_argument("Invalid (too small) buffer size for tree");
  }

  // no lock necessary here, as no other thread can see us yet
  const Meta* m = reinterpret_cast<const Meta*>(buffer.data());
  _data.meta = *m;
  // clear padded data
  _data.meta.padding = {0, 0};

  if (buffer.size() != allocationSize(meta().depth)) {
    throw std::invalid_argument("Unexpected buffer size for tree");
  }

  const char* p = buffer.data() + kMetaSize;
  const char* e = buffer.data() + buffer.size();
  uint64_t i = 0;
  while (p < e) {
    const Node* n = reinterpret_cast<const Node*>(p);
    if (n->count != 0 || n->hash != 0) {
      // only store nodes that are non-empty! this is an optimization so
      // we only need to allocate backing memory for shards that are
      // actually populated!
      node(i) = *n;
    }
    p += kNodeSize;
    ++i;
  }
#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif
}

template<typename Hasher, const uint64_t BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(Data&& data)
  : _data{std::move(data)} {}

template<typename Hasher, const uint64_t BranchingBits>
MerkleTree<Hasher, BranchingBits>::MerkleTree(
  const MerkleTree<Hasher, BranchingBits>& other) {
  // this is a protected constructor, and we get here only via clone().
  // in this case `other` is already properly locked

  // no lock necessary here for ourselves, as no other thread can see us yet
  _data.meta = Meta{other.meta().range_min, other.meta().range_max,
                    other.meta().depth,     other.meta().initial_range_min,
                    other.meta().summary,
                    /*padding*/ {0, 0}};

  uint64_t nodes_per_shard = shardSize(_data.meta.depth) / kNodeSize;

  _data.shards.reserve(other._data.shards.size());
  uint64_t offset = 0;
  for (const auto& it : other._data.shards) {
    if (it != nullptr) {
      // only copy nodes from shards which are actually populated
      for (uint64_t i = 0; i < nodes_per_shard; ++i) {
        if (!other.empty(offset + i)) {
          node(offset + i) = other.node(offset + i);
        }
      }
    }
    offset += nodes_per_shard;
  }

#ifdef SDB_DEV
  SDB_ASSERT(meta().depth == other.meta().depth);
  SDB_ASSERT(meta().range_min == other.meta().range_min);
  SDB_ASSERT(meta().range_max == other.meta().range_max);
  SDB_ASSERT(meta().initial_range_min == other.meta().initial_range_min);
  SDB_ASSERT(meta().summary == other.meta().summary);

#ifdef PARANOID_TREE_CHECKS
  uint64_t last = nodeCountAtDepth(meta().depth);
  for (uint64_t i = 0; i < last; ++i) {
    SDB_ASSERT(this->node(i) == other.node(i));
  }
#endif
#endif
}

template<typename Hasher, const uint64_t BranchingBits>
typename MerkleTree<Hasher, BranchingBits>::Meta&
MerkleTree<Hasher, BranchingBits>::meta() noexcept {
  // not thread-safe, lock buffer from outside
  return _data.meta;
}

template<typename Hasher, const uint64_t BranchingBits>
const MerkleTreeBase::Meta& MerkleTree<Hasher, BranchingBits>::meta()
  const noexcept {
  // not thread-safe, lock buffer from outside
  return _data.meta;
}

template<typename Hasher, const uint64_t BranchingBits>
MerkleTreeBase::Node& MerkleTree<Hasher, BranchingBits>::node(uint64_t index) {
  // not thread-safe, lock buffer from outside

  // determine shard
  uint64_t shard = shardForIndex(index);
  SDB_ASSERT(shard < numberOfShards());

  // lazily allocate backing memory
  _data.ensureShard(shard, kShardSize);

  SDB_ASSERT(_data.shards[shard] != nullptr);
  SDB_ASSERT(index < nodeCountAtDepth(meta().depth));
  SDB_ASSERT(kNodeSize * (index - shardBaseIndex(shard) + 1) <= kShardSize);
  return *(_data.shards[shard].get() + (index - shardBaseIndex(shard)));
}

template<typename Hasher, const uint64_t BranchingBits>
const MerkleTreeBase::Node& MerkleTree<Hasher, BranchingBits>::node(
  uint64_t index) const noexcept {
  // not thread-safe, lock buffer from outside
  SDB_ASSERT(index < nodeCountAtDepth(meta().depth));
  uint64_t shard = shardForIndex(index);
  if (_data.shards.size() <= shard || _data.shards[shard] == nullptr) {
    // return a shared empty node
    SDB_ASSERT(MerkleTreeBase::kEmptyNode.empty());
    return MerkleTreeBase::kEmptyNode;
  }
  return *(_data.shards[shard].get() + (index - shardBaseIndex(shard)));
}

void MerkleTreeBase::Meta::serialize(std::string& output,
                                     bool add_padding) const {
  // rangeMin / rangeMax / depth / initialRangeMin
  uint64_t value = basics::HostToLittle(range_min);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  value = basics::HostToLittle(range_max);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  value = basics::HostToLittle(depth);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  value = basics::HostToLittle(initial_range_min);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  // summary node count
  value = basics::HostToLittle(summary.count);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  // summary node hash
  value = basics::HostToLittle(summary.hash);
  output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

  if (add_padding) {
    value = 0;
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
    output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
  }
}

template<typename Hasher, const uint64_t BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::empty(uint64_t index) const noexcept {
  // not thread-safe, lock buffer from outside
  return this->node(index).empty();
}

template<typename Hasher, const uint64_t BranchingBits>
uint64_t MerkleTree<Hasher, BranchingBits>::index(uint64_t key) const noexcept {
  // not thread-safe, lock buffer from outside
  SDB_ASSERT(key >= meta().range_min);
  SDB_ASSERT(key < meta().range_max);

  uint64_t offset = key - meta().range_min;
  uint64_t chunk_size_at_depth =
    (meta().range_max - meta().range_min) /
    (static_cast<uint64_t>(1) << (BranchingBits * meta().depth));
  uint64_t chunk = offset / chunk_size_at_depth;

  return chunk;
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::modify(uint64_t key, bool is_insert) {
  // not thread-safe, shared-lock buffer from outside
  Hasher h;
  const uint64_t value = h.hash(key);

  // adjust bucket node
  bool success = modifyLocal(key, value, is_insert);
  if (!success) [[unlikely]] {
    throw std::invalid_argument("Tried to remove key that is not present.");
  }

  // adjust summary node
  modifyLocal(meta().summary, 1, value, is_insert);
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::modify(
  const std::vector<uint64_t>& keys, bool is_insert) {
  // not thread-safe, unique-lock buffer from outside
  Hasher h;
  uint64_t total_count = 0;
  uint64_t total_hash = 0;
  for (uint64_t key : keys) {
    uint64_t value = h.hash(key);
    bool success = modifyLocal(key, value, is_insert);
    if (!success && total_count > 0) [[unlikely]] {
      // roll back the changes we already made, using best effort
      SDB_ASSERT(!is_insert);
      for (uint64_t i = 0; i < total_count; ++i) {
        SDB_ASSERT(i < keys.size());
        uint64_t k = keys[i];
        [[maybe_unused]] bool rolled_back =
          modifyLocal(k, h.hash(k), !is_insert);
        SDB_ASSERT(rolled_back);
      }
      throw std::invalid_argument("Tried to remove key that is not present.");
    }
    ++total_count;
    total_hash ^= value;
  }

  // adjust summary node
  bool success =
    modifyLocal(meta().summary, total_count, total_hash, is_insert);
  SDB_ASSERT(success);
}

template<typename Hasher, const uint64_t BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::modifyLocal(Node& node, uint64_t count,
                                                    uint64_t value,
                                                    bool is_insert) noexcept {
  // only use via modify
  if (is_insert) {
    node.count += count;
  } else {
    if (node.count < count) [[unlikely]] {
      return false;
    }
    node.count -= count;
  }
  node.hash ^= value;

  SDB_ASSERT(node.count > 0 || node.hash == 0, "node count: ", node.count,
             ", hash: ", node.hash, ", count: ", count, ", value: ", value,
             ", isInsert: ", is_insert,
             ", isRoot: ", (&node == &(meta().summary)));
  return true;
}

template<typename Hasher, const uint64_t BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::modifyLocal(uint64_t key,
                                                    uint64_t value,
                                                    bool is_insert) {
  // only use via modify
  uint64_t index = this->index(key);
  SDB_ASSERT(&(this->node(index)) != &(meta().summary));
  return modifyLocal(this->node(index), 1, value, is_insert);
}

// The following method combines buckets for a growth operation to the
// right. It only does a factor of 2 and potentially a shift by one.
template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::leftCombine(bool with_shift) {
  // not thread-safe, lock nodes from outside
#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif

  // First to depth:
  const auto depth = meta().depth;
  const auto n = nodeCountAtDepth(depth);
  if (with_shift) {
    // 0 -> 0
    // 1, 2 -> 1
    // 3, 4 -> 2
    // 5, 6 -> 3
    // ...
    // n-1 -> n/2
    // empty -> n/2+1
    // ...
    // empty -> n
    for (uint64_t i = 2; i < n; ++i) {
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node((i + 1) / 2);

        SDB_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = {0, 0};
      }
    }
  } else {
    // 0, 1      -> 0
    // 2, 3      -> 1
    // 4, 5      -> 2
    // ...
    // n-2, n-1  -> n/2 - 1
    // empty     -> n/2
    // ...
    // empty     -> n - 1
    for (uint64_t i = 1; i < n; ++i) {
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node(i / 2);

        SDB_ASSERT(i % 2 != 0 || (dst.count == 0 && dst.hash == 0));

        SDB_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = {0, 0};
      }
    }
  }
#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::growRight(uint64_t key) {
  // not thread-safe, lock buffer from outside
#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif

  uint64_t depth = meta().depth;
  uint64_t range_min = meta().range_min;
  uint64_t range_max = meta().range_max;
  uint64_t initial_range_min = meta().initial_range_min;

  SDB_ASSERT(range_max > range_min);
  SDB_ASSERT(range_min <= initial_range_min);

  while (key >= range_max) {
    // someone else resized already while we were waiting for the lock
    // Furthermore, we can only grow by a factor of 2, so we might have
    // to grow multiple times

    SDB_ASSERT(range_min < range_max);
    const uint64_t width = range_max - range_min;
    SDB_ASSERT(number_utils::IsPowerOf2(width));

    if (width > std::numeric_limits<uint64_t>::max() - range_max) {
      // Oh dear, this would lead to overflow of uint64_t in rangeMax,
      // throw up our hands in despair:
      throw std::out_of_range(
        "Cannot grow MerkleTree because of overflow in rangeMax.");
    }
    const uint64_t keys_per_bucket = width / nodeCountAtDepth(depth);

    // First find out if we need to shift or not, this is for the
    // invariant 2:
    bool need_to_shift =
      (initial_range_min - range_min) % (2 * keys_per_bucket) != 0;

    leftCombine(need_to_shift);

    range_max += width;
    if (need_to_shift) {
      range_max -= keys_per_bucket;
      range_min -= keys_per_bucket;
    }

    SDB_ASSERT(range_max > range_min);
    SDB_ASSERT(number_utils::IsPowerOf2(range_max - range_min));
    meta().range_max = range_max;
    meta().range_min = range_min;

    SDB_ASSERT(meta().range_min < meta().range_max);

#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
    checkInternalConsistency();
#endif
#endif
  }
  SDB_ASSERT(key < meta().range_max);
}

// The following method combines buckets for a growth operation to the
// left. It only does a factor of 2 and potentially a shift by one.
template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::rightCombine(bool with_shift) {
  // not thread-safe, lock nodes from outside
#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif

  // First to depth:
  const auto depth = meta().depth;
  const auto n = nodeCountAtDepth(depth);
  if (with_shift) {
    // empty     -> 0
    // ...
    // empty     -> n/2 - 2
    // 0         -> n/2 - 1
    // 1, 2      -> n/2
    // 3, 4      -> n/2 + 1
    // 5, 6      -> n/2 + 2
    // ...
    // n-3, n-2  -> n - 2
    // n-1       -> n - 1
    for (uint64_t i = n - 3; /* i >= 0 */; --i) {
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node((n + i - 1) / 2);

        SDB_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = {0, 0};
      }
      if (i == 0) {
        break;
      }
    }
  } else {
    // empty     -> 0
    // ...
    // empty     -> n/2 - 1
    // 0, 1      -> n/2
    // 2, 3      -> n/2 + 1
    // 4, 5      -> n/2 + 2
    // ...
    // n-2, n-1  -> n - 1
    for (uint64_t i = n - 2; /* i >= 0 */; --i) {
      if (!this->empty(i)) {
        // Please note: If both src Nodes are empty for some dst,
        // nothing is combined. This is OK, for the following reason:
        // The respective dst Node was a src in a previous loop
        // iteration. Either it was empty then, then it can stay empty
        // in this case. If it wasn't empty, then the assignmend
        // `src // = { 0, 0 };` at the end of the loop has erased it, so
        // it is empty now, also good.
        Node& src = this->node(i);
        Node& dst = this->node((n + i) / 2);

        SDB_ASSERT(&src != &dst);
        dst.count += src.count;
        dst.hash ^= src.hash;
        // clear source node
        src = {0, 0};
      }
      if (i == 0) {
        break;
      }
    }
  }
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::growLeft(uint64_t key) {
  // not thread-safe, lock buffer from outside
#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
  checkInternalConsistency();
#endif
#endif

  uint64_t depth = meta().depth;
  uint64_t range_min = meta().range_min;
  uint64_t range_max = meta().range_max;
  uint64_t initial_range_min = meta().initial_range_min;

  while (key < range_min) {
    // someone else resized already while we were waiting for the lock
    // Furthermore, we can only grow by a factor of 2, so we might have
    // to grow multiple times

    const uint64_t width = range_max - range_min;
    const uint64_t keys_per_bucket = width / nodeCountAtDepth(depth);

    if (width > range_min) {
      // Oh dear, this would lead to underflow of uint64_t in rangeMin,
      // throw up our hands in despair:
      throw std::out_of_range(
        "Cannot grow MerkleTree because of underflow in rangeMin.");
    }

    SDB_ASSERT(range_min < range_max);

    // First find out if we need to shift or not, this is for the
    // invariant 2:
    bool need_to_shift =
      (initial_range_min - range_min) % (2 * keys_per_bucket) != 0;

    rightCombine(need_to_shift);

    SDB_ASSERT(range_min >= width);

    range_min -= width;
    if (need_to_shift) {
      range_max += keys_per_bucket;
      range_min += keys_per_bucket;
    }
    SDB_ASSERT(range_max > range_min);
    SDB_ASSERT(number_utils::IsPowerOf2(range_max - range_min));
    meta().range_max = range_max;
    meta().range_min = range_min;

    SDB_ASSERT(meta().range_min < meta().range_max);

#ifdef SDB_DEV
#ifdef PARANOID_TREE_CHECKS
    checkInternalConsistency();
#endif
#endif
  }
  SDB_ASSERT(key >= meta().range_min);
}

template<typename Hasher, const uint64_t BranchingBits>
bool MerkleTree<Hasher, BranchingBits>::equalAtIndex(
  const MerkleTree<Hasher, BranchingBits>& other,
  uint64_t index) const noexcept {
  // not fully thread-safe, lock nodes from outside
  return (this->node(index) == other.node(index));
}

template<typename Hasher, const uint64_t BranchingBits>
std::pair<uint64_t, uint64_t> MerkleTree<Hasher, BranchingBits>::chunkRange(
  uint64_t chunk, uint64_t depth) const {
  // not thread-safe, lock buffer from outside
  uint64_t range_min = meta().range_min;
  uint64_t range_max = meta().range_max;
  uint64_t chunk_size_at_depth =
    (range_max - range_min) /
    (static_cast<uint64_t>(1) << (BranchingBits * depth));
  return std::make_pair(range_min + (chunk_size_at_depth * chunk),
                        range_min + (chunk_size_at_depth * (chunk + 1)) - 1);
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::checkInternalConsistency() const {
  SDB_IF_FAILURE("MerkleTree::skipConsistencyCheck") { return; }

  // not thread-safe, lock buffer from outside

  // validate meta data
  uint64_t depth = meta().depth;
  uint64_t range_min = meta().range_min;
  uint64_t range_max = meta().range_max;
  uint64_t initial_range_min = meta().initial_range_min;
  uint64_t width = range_max - range_min;

  if (depth < 2) {
    throw std::invalid_argument("Invalid tree depth");
  }
  if (range_min >= range_max) {
    throw std::invalid_argument("Invalid tree rangeMin / rangeMax");
  }
  if (!number_utils::IsPowerOf2(range_max - range_min)) {
    throw std::invalid_argument(
      "Expecting difference between min and max to be power of 2");
  }
  if ((initial_range_min - range_min) %
        ((range_max - range_min) / nodeCountAtDepth(depth)) !=
      0) {
    throw std::invalid_argument(
      "Expecting difference between initial min and min to be divisible by "
      "(max-min)/nodeCountAt(depth)");
  }

  SDB_ASSERT(width > 0);
  SDB_ASSERT(numberOfShards() > 0);
  size_t shard_size = width / numberOfShards();
  SDB_ASSERT(width % shard_size == 0);

  uint64_t total_count = 0;
  uint64_t total_hash = 0;
  uint64_t last = nodeCountAtDepth(depth);
  for (uint64_t i = 0; i < last; ++i) {
    const Node& n = this->node(i);
    SDB_ASSERT(n.count != 0 || n.hash == 0);
    total_count += n.count;
    total_hash ^= n.hash;
  }

  if (total_count != meta().summary.count) {
    throw std::invalid_argument("Inconsistent count values in tree");
  }

  if (total_hash != meta().summary.hash) {
    throw std::invalid_argument("Inconsistent hash values in tree");
  }
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serializeMeta(std::string& output,
                                                      bool add_padding) const {
  SDB_ASSERT(output.empty());
  meta().serialize(output, add_padding);
  SDB_ASSERT(output.size() ==
             kMetaSize - (add_padding ? 0 : sizeof(Meta::Padding)));
}

template<typename Hasher, const uint64_t BranchingBits>
void MerkleTree<Hasher, BranchingBits>::serializeNodes(std::string& output,
                                                       bool all) const {
  uint64_t depth = meta().depth;
  uint64_t last = nodeCountAtDepth(depth);
  for (uint64_t index = 0; index < last; ++index) {
    const Node& src = this->node(index);
    SDB_ASSERT(src.count > 0 || src.hash == 0, "node count: ", src.count,
               ", hash: ", src.hash);

    // serialize only the non-zero buckets
    if (src.count == 0 && !all) {
      continue;
    }

    if (!all) {
      // index
      uint32_t value = basics::HostToLittle(static_cast<uint32_t>(index));
      output.append(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
    }

    // count / hash
    {
      uint64_t value = basics::HostToLittle(src.count);
      output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));

      value = basics::HostToLittle(src.hash);
      output.append(reinterpret_cast<const char*>(&value), sizeof(uint64_t));
    }
  }
}

/// INSTANTIATIONS
template class MerkleTree<FnvHashProvider, 3>;

}  // namespace sdb::containers
