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

#include "storage_engine/health_data.h"

#include <vpack/builder.h>
#include <vpack/value.h>

#include "basics/errors.h"

using namespace sdb;

/*static*/ sdb::HealthData sdb::HealthData::fromVPack(vpack::Slice slice) {
  HealthData result;
  if (slice.isObject()) {
    auto code = ERROR_OK;
    std::string message;

    if (vpack::Slice status = slice.get("status");
        status.isString() && status.isEqualString("BAD")) {
      code = ERROR_FAILED;
    }
    if (vpack::Slice msg = slice.get("message"); msg.isString()) {
      message = msg.stringView();
    }
    result.res.reset(code, std::move(message));

    if (vpack::Slice bg = slice.get("backgroundError"); bg.isBool()) {
      result.background_error = bg.getBool();
    }

    if (vpack::Slice fdsb = slice.get("freeDiskSpaceBytes"); fdsb.isNumber()) {
      result.free_disk_space_bytes =
        fdsb.getNumber<decltype(free_disk_space_bytes)>();
    }

    if (vpack::Slice fdsp = slice.get("freeDiskSpacePercent");
        fdsp.isNumber()) {
      result.free_disk_space_percent =
        fdsp.getNumber<decltype(free_disk_space_percent)>();
    }
  }

  return result;
}

void sdb::HealthData::toVPack(vpack::Builder& builder,
                              bool with_details) const {
  builder.add("health", vpack::Value(vpack::ValueType::Object));
  builder.add("status", res.ok() ? "GOOD" : "BAD");
  if (res.fail()) {
    builder.add("message", res.errorMessage());
  }
  builder.add("backgroundError", background_error);
  if (with_details) {
    builder.add("freeDiskSpaceBytes", free_disk_space_bytes);
    builder.add("freeDiskSpacePercent", free_disk_space_percent);
  }
  builder.close();
}
