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

#include <vpack/builder.h>
#include <vpack/slice.h>

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "basics/common.h"
#include "maskings/collection.h"
#include "maskings/parse_result.h"

namespace sdb::maskings {

struct MaskingsResult;

class Maskings {
 public:
  static MaskingsResult fromFile(const std::string&);

  bool shouldDumpStructure(const std::string& name);
  bool shouldDumpData(const std::string& name);
  void mask(const std::string& name, vpack::Slice data,
            vpack::Builder& builder) const;

  uint64_t randomSeed() const noexcept { return _random_seed; }

 private:
  ParseResult<Maskings> parse(vpack::Slice def);
  void maskedItem(const Collection& collection,
                  std::vector<std::string_view>& path, vpack::Slice data,
                  vpack::Builder& out, std::string& buffer) const;
  void addMaskedArray(const Collection& collection,
                      std::vector<std::string_view>& path, vpack::Slice data,
                      vpack::Builder& builder, std::string& buffer) const;
  void addMaskedObject(const Collection& collection,
                       std::vector<std::string_view>& path, vpack::Slice data,
                       vpack::Builder& builder, std::string& buffer) const;
  void addMasked(const Collection& collection, vpack::Builder& out,
                 vpack::Slice data) const;

 private:
  std::map<std::string, Collection, std ::less<>> _collections;
  bool _has_default_collection = false;
  Collection _default_collection;
  uint64_t _random_seed = 0;
};

struct MaskingsResult {
  enum StatusCode : int {
    kValid,
    kCannotParseFile,
    kCannotReadFile,
    kIllegalDefinition
  };

  MaskingsResult(StatusCode s, const std::string& m)
    : status(s), message(m), maskings(nullptr) {}
  explicit MaskingsResult(std::unique_ptr<Maskings>&& m)
    : status(StatusCode::kValid), maskings(std::move(m)) {}

  StatusCode status;
  std::string message;
  std::unique_ptr<Maskings> maskings;
};

}  // namespace sdb::maskings
