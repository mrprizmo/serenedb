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

#include "iresearch/formats/formats.hpp"

namespace irs {

class ColumnFinalizer {
 public:
  using payload_finalizer_f = absl::AnyInvocable<void(DataOutput&)>;
  using name_finalizer_f = absl::AnyInvocable<std::string_view()>;

  ColumnFinalizer() = default;
  ColumnFinalizer(const ColumnFinalizer&) = default;
  ColumnFinalizer(ColumnFinalizer&&) = default;

  ColumnFinalizer& operator=(const ColumnFinalizer&) = default;
  ColumnFinalizer& operator=(ColumnFinalizer&&) = default;

  explicit ColumnFinalizer(payload_finalizer_f finalizer,
                           name_finalizer_f name_finalizer)
    : _finalizer(std::move(finalizer)),
      _name_finalizer(std::move(name_finalizer)) {}

  std::string_view GetName() {
    return _name_finalizer ? _name_finalizer() : std::string_view{};
  }

  void Finalize(DataOutput& writer) {
    if (_finalizer) {
      _finalizer(writer);
    }
  }

  payload_finalizer_f&& ExtractPayloadFinalizer() {
    return std::move(_finalizer);
  }

  name_finalizer_f&& ExtractNameFinalizer() {
    return std::move(_name_finalizer);
  }

 private:
  payload_finalizer_f _finalizer;
  name_finalizer_f _name_finalizer;
};

}  // namespace irs
