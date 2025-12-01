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

#include "collection.h"

#include <absl/strings/str_cat.h>
#include <vpack/builder.h>
#include <vpack/iterator.h>
#include <vpack/parser.h>
#include <vpack/slice.h>

using namespace sdb;
using namespace sdb::maskings;

ParseResult<Collection> Collection::parse(Maskings* maskings,
                                          vpack::Slice def) {
  if (!def.isObject()) {
    return ParseResult<Collection>(
      ParseResult<Collection>::kParseFailed,
      "expecting an object for collection definition");
  }

  std::string_view type;
  std::vector<AttributeMasking> attributes;

  for (auto entry : vpack::ObjectIterator(def, false)) {
    auto key = entry.key.stringView();

    if (key == "type") {
      auto value = entry.value();
      if (!value.isString()) {
        return ParseResult<Collection>(
          ParseResult<Collection>::kIllegalParameter,
          "expecting a string for collection type");
      }

      type = value.stringView();
    } else if (key == "maskings") {
      auto value = entry.value();
      if (!value.isArray()) {
        return ParseResult<Collection>(
          ParseResult<Collection>::kIllegalParameter,
          "expecting an array for collection maskings");
      }

      for (const auto& mask : vpack::ArrayIterator(value)) {
        ParseResult<AttributeMasking> am =
          AttributeMasking::parse(maskings, mask);

        if (am.status != ParseResult<AttributeMasking>::kValid) {
          return ParseResult<Collection>(
            (ParseResult<Collection>::StatusCode)(int)am.status, am.message);
        }

        attributes.push_back(am.result);
      }
    }
  }

  CollectionSelection selection = CollectionSelection::FULL;

  if (type == "full") {
    selection = CollectionSelection::FULL;
  } else if (type == "exclude") {
    selection = CollectionSelection::EXCLUDE;
  } else if (type == "masked") {
    selection = CollectionSelection::MASKED;
  } else if (type == "structure") {
    selection = CollectionSelection::STRUCTURE;
  } else {
    return ParseResult<Collection>(
      ParseResult<Collection>::kUnknownType,
      absl::StrCat("found unknown collection type '", type, "'"));
  }

  return ParseResult<Collection>(Collection(selection, attributes));
}

MaskingFunction* Collection::masking(
  const std::vector<std::string_view>& path) const {
  for (const auto& m : _maskings) {
    if (m.match(path)) {
      return m.func();
    }
  }

  return nullptr;
}
