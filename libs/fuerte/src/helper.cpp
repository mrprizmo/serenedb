////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Jan Christoph Uhde
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <fuerte/helper.h>
#include <string.h>
#include <vpack/iterator.h>
#include <vpack/slice.h>

#include "http.h"

namespace sdb::fuerte {

std::string ExtractPathParameters(std::string_view p, StringMap& params) {
  size_t pos = p.rfind('?');
  if (pos == p.npos) {
    return std::string(p);
  }

  std::string result(p.substr(0, pos));

  while (pos != p.npos && pos + 1 < p.length()) {
    size_t pos2 = p.find('=', pos + 1);
    if (pos2 == p.npos) {
      break;
    }
    std::string_view key = p.substr(pos + 1, pos2 - pos - 1);
    pos = p.find('&', pos2 + 1);  // points to next '&' or string::npos
    std::string_view value =
      pos == p.npos ? p.substr(pos2 + 1) : p.substr(pos2 + 1, pos - pos2 - 1);
    params.emplace(http::UrlDecode(key), http::UrlDecode(value));
  }

  return result;
}

}  // namespace sdb::fuerte
