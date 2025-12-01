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

#include <memory>

#include "basics/noncopyable.hpp"
#include "compression.hpp"
#include "string.hpp"

namespace irs::compression {

struct Lz4 {
  static constexpr std::string_view type_name() noexcept {
    return "irs::compression::lz4";
  }

  static void init();
  static compression::Compressor::ptr compressor(const Options& opts);
  static compression::Decompressor::ptr decompressor();

  class Lz4Compressor : public compression::Compressor {
   public:
    explicit Lz4Compressor(int acceleration = 0) noexcept
      : _acceleration(acceleration) {}

    int acceleration() const noexcept { return _acceleration; }

    bytes_view compress(byte_type* src, size_t size, bstring& out) final
      IRS_ATTRIBUTE_NONNULL(2);

   private:
    const int _acceleration{0};  // 0 - default acceleration
  };

  class Lz4Decompressor : public compression::Decompressor {
   public:
    bytes_view decompress(const byte_type* src, size_t src_size, byte_type* dst,
                          size_t dst_size) final IRS_ATTRIBUTE_NONNULL(2);
  };
};

}  // namespace irs::compression
