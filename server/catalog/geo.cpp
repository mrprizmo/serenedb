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

#include "catalog/geo.h"

#include <s2/s2latlng.h>
#include <vpack/builder.h>
#include <vpack/slice.h>

#include "basics/logger/logger.h"
#include "catalog/search_common.h"
#include "geo/geo_json.h"
#include "geo/shape_container.h"

namespace sdb::search {

template<Parsing P>
bool ParseShape(vpack::Slice vpack, geo::ShapeContainer& region,
                std::vector<S2LatLng>& cache, geo::coding::Options options,
                Encoder* encoder) {
  SDB_ASSERT(encoder == nullptr || encoder->length() == 0);
  Result r;
  if (vpack.isArray()) {
    r = geo::json::ParseCoordinates<P != Parsing::FromIndex>(vpack, region,
                                                             /*geoJson=*/true,
                                                             options, encoder);
  } else if constexpr (P == Parsing::OnlyPoint) {
    auto parse_point = [&] {
      S2LatLng lat_lng;
      r = geo::json::ParsePoint(vpack, lat_lng);
      if (r.ok() && encoder != nullptr) {
        SDB_ASSERT(options != geo::coding::Options::Invalid);
        SDB_ASSERT(encoder->avail() >= sizeof(uint8_t));
        // We store type, because ParseCoordinates store it
        encoder->put8(0);  // In store to column we will remove it
        if (geo::coding::IsOptionsS2(options)) {
          auto point = lat_lng.ToPoint();
          geo::EncodePoint(*encoder, point);
          return point;
        } else {
          geo::EncodeLatLng(*encoder, lat_lng, options);
        }
      } else if (r.ok() && options == geo::coding::Options::S2LatLngU32) {
        geo::ToLatLngU32(lat_lng);
      }
      return lat_lng.ToPoint();
    };
    if (r.ok()) {
      region.reset(parse_point(), options);
    }
  } else {
    r = geo::json::ParseRegion<P != Parsing::FromIndex>(vpack, region, cache,
                                                        options, encoder);
  }
  if (P != Parsing::FromIndex && r.fail()) {
    SDB_DEBUG(
      "xxxxx", Logger::SEARCH,
      "Failed to parse value as GEO JSON or array of coordinates, error '",
      r.errorMessage(), "'");
    return false;
  }
  return true;
}

template bool ParseShape<Parsing::FromIndex>(vpack::Slice slice,
                                             geo::ShapeContainer& shape,
                                             std::vector<S2LatLng>& cache,
                                             geo::coding::Options options,
                                             Encoder* encoder);
template bool ParseShape<Parsing::OnlyPoint>(vpack::Slice slice,
                                             geo::ShapeContainer& shape,
                                             std::vector<S2LatLng>& cache,
                                             geo::coding::Options options,
                                             Encoder* encoder);
template bool ParseShape<Parsing::GeoJson>(vpack::Slice slice,
                                           geo::ShapeContainer& shape,
                                           std::vector<S2LatLng>& cache,
                                           geo::coding::Options options,
                                           Encoder* encoder);

void ToVPack(vpack::Builder& builder, S2LatLng point) {
  SDB_ASSERT(point.is_valid());
  // false because with false it's smaller
  // in general we want only doubles, but format requires it should be array
  // so we generate most smaller vpack array
  builder.openArray(false);
  builder.add(point.lng().degrees());
  builder.add(point.lat().degrees());
  builder.close();
  SDB_ASSERT(builder.slice().isArray());
  SDB_ASSERT(builder.slice().head() == 0x02);
}

}  // namespace sdb::search
