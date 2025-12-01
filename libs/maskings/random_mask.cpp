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

#include "random_mask.h"

#include <memory>

#include "basics/random/random_generator.h"
#include "maskings/maskings.h"

using namespace sdb;
using namespace sdb::maskings;

ParseResult<AttributeMasking> RandomMask::create(Path path, Maskings* maskings,
                                                 vpack::Slice /*def*/) {
  return ParseResult<AttributeMasking>(
    AttributeMasking(std::move(path), std::make_shared<RandomMask>(maskings)));
}

void RandomMask::mask(bool value, vpack::Builder& out, std::string&) const {
  auto result = random::Interval(uint16_t{1});
  out.add(result == 0);
}

void RandomMask::mask(int64_t, vpack::Builder& out, std::string&) const {
  auto result = random::Interval(-1000, 1000);
  out.add(result);
}

void RandomMask::mask(double, vpack::Builder& out, std::string&) const {
  auto result = random::Interval(-1000, 1000);
  out.add(result / 100.0);
}
