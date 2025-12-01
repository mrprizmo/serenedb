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

#include <vpack/slice.h>

#include <iresearch/analysis/tokenizers.hpp>

namespace sdb {
namespace search {

class IdentityAnalyzer final : public irs::StringTokenizer {
 public:
  static bool normalize(std::string_view /*args*/, std::string& out);

  static ptr make(std::string_view /*args*/);

  static bool normalize_json(std::string_view /*args*/, std::string& out);

  static ptr make_json(std::string_view /*args*/);
};

}  // namespace search
}  // namespace sdb
