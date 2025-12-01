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

#pragma once

#include <absl/strings/cord.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>

#include <memory>

#include "basics/sink.h"
#include "basics/string_utils.h"

namespace sdb::basics {

class StringBuffer : public StrSink {
 public:
  bool empty() const { return _impl.empty(); }
  size_t size() const { return _impl.size(); }

  const char* data() const { return _impl.data(); }
  char* data() { return _impl.data(); }

  void clear() { _impl.clear(); }

  void erase(size_t pos = 0, size_t n = std::string_view::npos) {
    _impl.erase(pos, n);
  }

  void reserve(size_t size) { _impl.reserve(size); }

  void resize(size_t size) { basics::StrResize(_impl, size); }

  void resizeAdditional(size_t true_size, size_t needed) {
    basics::StrResizeAmortized(_impl, true_size + needed);
  }

  void append(std::string_view s) { PushStr(s); }
};

using StringBufferPtr = std::unique_ptr<StringBuffer>;

}  // namespace sdb::basics
