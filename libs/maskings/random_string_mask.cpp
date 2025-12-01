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

#include "random_string_mask.h"

#include <absl/strings/escaping.h>
#include <vpack/iterator.h>
#include <vpack/slice.h>

#include <memory>

#include "basics/string_utils.h"
#include "maskings/maskings.h"

using namespace sdb;
using namespace sdb::maskings;

ParseResult<AttributeMasking> RandomStringMask::create(Path path,
                                                       Maskings* maskings,
                                                       vpack::Slice /*def*/) {
  return ParseResult<AttributeMasking>(AttributeMasking(
    std::move(path), std::make_shared<RandomStringMask>(maskings)));
}

void RandomStringMask::mask(std::string_view data, vpack::Builder& out,
                            std::string& buffer) const {
  uint64_t len = data.size();
  uint64_t hash;

  hash = VPACK_HASH(data.data(), data.size(), _maskings->randomSeed());

  std::string hash64 =
    absl::Base64Escape(std::string_view{(const char*)&hash, sizeof(hash)});

  buffer.clear();
  buffer.reserve(len);
  buffer.append(hash64);

  if (buffer.size() < len) {
    while (buffer.size() < len) {
      buffer.append(hash64);
    }

    buffer.resize(len);
  }

  out.add(buffer);
}
