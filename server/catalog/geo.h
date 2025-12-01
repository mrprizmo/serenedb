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

#include <s2/s2region_term_indexer.h>

#include "geo/coding.h"

namespace vpack {

class Slice;
class Builder;

}  // namespace vpack
namespace sdb {
namespace geo {

class ShapeContainer;

}  // namespace geo
namespace search {

struct GeoOptions {
  static constexpr int32_t kMinCells = 0;  // TODO(mbkkt) It's looks incorrect
  static constexpr int32_t kMaxCells = std::numeric_limits<int32_t>::max();
  static constexpr int32_t kMinLevel = 0;  // TODO(mbkkt) Is it correct?
  static constexpr int32_t kMaxLevel = S2CellId::kMaxLevel;
  static constexpr int32_t kMinLevelMod = 1;
  static constexpr int32_t kMaxLevelMod = 3;

  static constexpr int32_t kDefaultMaxCells = 20;
  static constexpr int32_t kDefaultMinLevel = 4;
  static constexpr int32_t kDefaultMaxLevel = 23;  // ~1m
  static constexpr int8_t kDefaultLevelMod = 1;

  // TODO(mbkkt) different maxCells can be set on every insertion/querying
  int32_t max_cells{kDefaultMaxCells};
  int32_t min_level{kDefaultMinLevel};
  int32_t max_level{kDefaultMaxLevel};
  int8_t level_mod{kDefaultLevelMod};
  bool optimize_for_space{false};
};

inline S2RegionTermIndexer::Options S2Options(const GeoOptions& opts,
                                              bool points_only) {
  S2RegionTermIndexer::Options s2opts;
  s2opts.set_max_cells(opts.max_cells);
  s2opts.set_min_level(opts.min_level);
  s2opts.set_max_level(opts.max_level);
  s2opts.set_level_mod(opts.level_mod);
  s2opts.set_optimize_for_space(opts.optimize_for_space);
  s2opts.set_index_contains_points_only(points_only);

  return s2opts;
}

enum class Parsing : uint8_t {
  FromIndex = 0,
  OnlyPoint,
  GeoJson,
};

template<Parsing P>
bool ParseShape(vpack::Slice slice, geo::ShapeContainer& shape,
                std::vector<S2LatLng>& cache, geo::coding::Options options,
                Encoder* encoder);

void ToVPack(vpack::Builder& builder, S2LatLng point);

}  // namespace search
}  // namespace sdb
