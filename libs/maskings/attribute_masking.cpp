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

#include "attribute_masking.h"

#include <absl/strings/str_cat.h>

#include "basics/string_utils.h"
#include "maskings/random_mask.h"
#include "maskings/random_string_mask.h"

using namespace sdb;
using namespace sdb::maskings;

void sdb::maskings::InstallMaskings() {
  AttributeMasking::installMasking("randomString", RandomStringMask::create);
  AttributeMasking::installMasking("random", RandomMask::create);
}

ParseResult<AttributeMasking> AttributeMasking::parse(Maskings* maskings,
                                                      vpack::Slice def) {
  if (!def.isObject()) {
    return ParseResult<AttributeMasking>(
      ParseResult<AttributeMasking>::kParseFailed,
      "expecting an object for collection definition");
  }

  std::string_view path;
  std::string type;

  for (auto entry : vpack::ObjectIterator(def, false)) {
    auto key = entry.key.stringView();

    if (key == "type") {
      auto v = entry.value();
      if (!v.isString()) {
        return ParseResult<AttributeMasking>(
          ParseResult<AttributeMasking>::kIllegalParameter,
          "type must be a string");
      }

      type = v.stringView();
    } else if (key == "path") {
      auto v = entry.value();
      if (!v.isString()) {
        return ParseResult<AttributeMasking>(
          ParseResult<AttributeMasking>::kIllegalParameter,
          "path must be a string");
      }

      path = v.stringView();
    }
  }

  if (path.empty()) {
    return ParseResult<AttributeMasking>(
      ParseResult<AttributeMasking>::kIllegalParameter,
      "path must not be empty");
  }

  ParseResult<Path> ap = Path::parse(path);

  if (ap.status != ParseResult<Path>::kValid) {
    return ParseResult<AttributeMasking>(
      (ParseResult<AttributeMasking>::StatusCode)(int)ap.status, ap.message);
  }

  const auto& it = gMaskings.find(type);

  if (it == gMaskings.end()) {
    return ParseResult<AttributeMasking>(
      ParseResult<AttributeMasking>::kUnknownType,
      absl::StrCat("unknown attribute masking type '", type, "'"));
  }

  return it->second(ap.result, maskings, def);
}

bool AttributeMasking::match(const std::vector<std::string_view>& path) const {
  return _path.match(path);
}
