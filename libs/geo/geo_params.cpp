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

#include "geo/geo_params.h"

#include <absl/strings/str_cat.h>
#include <s2/s2earth.h>
#include <s2/s2metrics.h>
#include <vpack/builder.h>
#include <vpack/slice.h>

namespace sdb::geo {

RegionCoverParams::RegionCoverParams()
  : max_num_cover_cells(kMaxNumCoverCellsDefault),
    worst_indexed_level(
      S2::kAvgEdge.GetClosestLevel(S2Earth::KmToRadians(600))),
    best_indexed_level(
      S2::kAvgEdge.GetClosestLevel(S2Earth::MetersToRadians(100.0))) {
  // optimize levels for buildings, points are converted without S2RegionCoverer
}

/// read the options from a vpack::Slice
void RegionCoverParams::fromVPack(vpack::Slice slice) {
  SDB_ASSERT(slice.isObject());
  vpack::Slice v;
  if ((v = slice.get("maxNumCoverCells")).isNumber<int>()) {
    max_num_cover_cells = v.getNumber<int>();
  }
  if ((v = slice.get("worstIndexedLevel")).isNumber<int>()) {
    worst_indexed_level = v.getNumber<int>();
  }
  if ((v = slice.get("bestIndexedLevel")).isNumber<int>()) {
    best_indexed_level = v.getNumber<int>();
  }
}

/// add the options to an opened vpack:: builder
void RegionCoverParams::toVPack(vpack::Builder& builder) const {
  SDB_ASSERT(builder.isOpenObject());
  builder.add("maxNumCoverCells", max_num_cover_cells);
  builder.add("worstIndexedLevel", worst_indexed_level);
  builder.add("bestIndexedLevel", best_indexed_level);
}

S2RegionCoverer::Options RegionCoverParams::regionCovererOpts() const {
  S2RegionCoverer::Options opts;
  opts.set_max_cells(max_num_cover_cells);  // This is a soft limit
  opts.set_min_level(worst_indexed_level);  // Levels are a strict limit
  opts.set_max_level(best_indexed_level);
  return opts;
}

double QueryParams::minDistanceRad() const noexcept {
  return MetersToRadians(min_distance);
}

double QueryParams::maxDistanceRad() const noexcept {
  return MetersToRadians(max_distance);
}

std::string QueryParams::toString() const {
  auto t = [](bool x) -> std::string_view { return x ? "true" : "false"; };
  return absl::StrCat(  // clang-format off
     "minDistance: ", min_distance,
    " incl: ", t(min_inclusive),
    " maxDistance: ", max_distance,
    " incl: ", t(max_inclusive),
    " distanceRestricted: ", t(distance_restricted),
    " sorted: ", t(sorted),
    " ascending: ", t(ascending),
    " origin: ", origin.lng().degrees(), " , ", origin.lat().degrees(),
    " pointsOnly: ", t(points_only),
    " limit: ", limit,
    " filterType: ", static_cast<int>(filter_type),
    " filterShape: ", static_cast<int>(filter_shape.type())
  );  // clang-format on
}

}  // namespace sdb::geo
