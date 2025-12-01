////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2025 SereneDB GmbH, Berlin, Germany
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
/// Copyright holder is SereneDB GmbH, Berlin, Germany
////////////////////////////////////////////////////////////////////////////////

#include "mangling.h"

#include "basics/assert.h"

namespace sdb::search::mangling {

std::string_view DemangleNested(std::string_view name, std::string& buf) {
  const auto end = std::end(name);
  if (end == std::find(std::begin(name), end, kNested)) {
    return name;
  }

  auto prev = std::begin(name);
  auto cur = prev;

  buf.clear();

  for (auto end = std::end(name); cur != end; ++cur) {
    if (kNested == *cur) {
      buf.append(prev, cur);
      prev = cur + 1;
    }
  }
  buf.append(prev, cur);

  return buf;
}

std::string_view DemangleType(std::string_view name) noexcept {
  if (name.empty()) {
    return {};
  }

  for (size_t i = name.size() - 1;; --i) {
    if (name[i] <= kAnalyzer) {
      return {name.data(), i};
    }
    if (i == 0) {
      break;
    }
  }

  return name;
}

std::string_view ExtractAnalyzerName(std::string_view field_name) {
  auto analyzer_index = field_name.find(kAnalyzer);
  if (analyzer_index != std::string_view::npos) {
    ++analyzer_index;
    SDB_ASSERT(analyzer_index != field_name.size());
    return field_name.substr(analyzer_index);
  }
  return {};
}

}  // namespace sdb::search::mangling
