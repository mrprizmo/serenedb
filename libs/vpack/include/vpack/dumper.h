////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <cstdint>
#include <string>
#include <string_view>

#include "vpack/common.h"
#include "vpack/options.h"
#include "vpack/slice.h"

namespace vpack {

// Dumps VPack into a JSON output string
// TODO(gnusi) ctad?
template<typename Sink>
class Dumper {
 public:
  Sink* sink;
  const Options* options;

  Dumper(const Dumper&) = delete;
  Dumper& operator=(const Dumper&) = delete;

  explicit Dumper(Sink* sink, const Options* options = &Options::gDefaults);

  void Dump(Slice slice);

  void DumpStr(std::string_view str);
  void DumpU64(uint64_t u);
  void DumpI64(int64_t i);

 private:
  void DumpSlice(Slice slice);

  void Indent();

  void HandleUnsupportedType(Slice slice);

  int _indentation = 0;
};

template<typename Sink>
void Dump(Slice slice, Sink* sink,
          const Options* options = &Options::gDefaults) {
  Dumper dumper{sink, options};
  dumper.Dump(slice);
}

}  // namespace vpack
