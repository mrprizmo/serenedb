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

#include "translator.h"

#include <absl/strings/str_split.h>

#include "basics/containers/flat_hash_map.h"
#include "basics/files.h"
#include "basics/string_utils.h"
#include "basics/thread.h"

namespace sdb {
namespace {

containers::FlatHashMap<std::string, std::string> gEnvironment;
}

void options::DefineEnvironment(std::string_view key_values) {
  auto kvs = absl::StrSplit(key_values, ',');

  for (const auto& key_value : kvs) {
    std::string key;
    std::string value;

    const size_t delim = key_value.find('=');

    if (delim == std::string::npos) {
      key = key_value;
    } else {
      key = key_value.substr(0, delim);
      value = key_value.substr(delim + 1);
    }

    gEnvironment[key] = value;
  }
}

std::string sdb::options::EnvironmentTranslator(std::string_view value,
                                                const char* binary_path) {
  if (value.empty()) {
    return std::string{value};
  }

  const char* p = value.data();
  const char* e = p + value.size();

  std::string result;

  for (const char* q = p; q < e; q++) {
    if (*q == '@') {
      q++;

      if (*q == '@') {
        result.push_back('@');
      } else {
        const char* t = q;

        for (; q < e && *q != '@'; q++)
          ;

        if (q < e) {
          std::string k = std::string(t, q);
          std::string vv;
          if (!SdbGETENV(k.c_str(), vv) || (vv.length() == 0)) {
            auto iter = gEnvironment.find(k);

            if (iter != gEnvironment.end()) {
              vv = iter->second;
            }
          }

          if (vv.length() == 0) {
            if (k == "PID") {
              vv = std::to_string(Thread::currentProcessId());
            }
          }

          result += vv;
        } else {
          result += std::string(t - 1);
        }
      }
    } else {
      result.push_back(*q);
    }
  }

  return result;
}

}  // namespace sdb
