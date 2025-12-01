////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "basics/noncopyable.hpp"
#include "compression.hpp"
#include "string.hpp"

namespace irs {
namespace compression {

class DeltaCompressor : public Compressor, private util::Noncopyable {
 public:
  bytes_view compress(byte_type* src, size_t size, bstring& out) final;
};

class DeltaDecompressor : public Decompressor, private util::Noncopyable {
 public:
  /// @returns bytes_view::NIL in case of error
  bytes_view decompress(const byte_type* src, size_t src_size, byte_type* dst,
                        size_t dst_size) final;
};

struct Delta {
  static constexpr std::string_view type_name() noexcept {
    return "irs::compression::delta";
  }

  static void init();
  static compression::Compressor::ptr compressor(const Options& opts);
  static compression::Decompressor::ptr decompressor();
};

}  // namespace compression
}  // namespace irs
