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

#include <cstddef>
#include <cstdint>

#include "basics/error_code.h"

typedef union LZ4_stream_u LZ4_stream_t;  // NOLINT

namespace sdb::encoding {

LZ4_stream_t* MakeLZ4Stream();

template<typename T>
[[nodiscard]] ErrorCode GZipUncompress(const uint8_t* compressed,
                                       size_t compressed_length,
                                       T& uncompressed);

template<typename T>
[[nodiscard]] ErrorCode ZLibInflate(const uint8_t* compressed,
                                    size_t compressed_length, T& uncompressed);

template<typename T>
[[nodiscard]] ErrorCode Lz4Uncompress(const uint8_t* compressed,
                                      size_t compressed_length,
                                      T& uncompressed);

template<typename T>
[[nodiscard]] ErrorCode GZipCompress(const uint8_t* uncompressed,
                                     size_t uncompressed_length, T& compressed);

template<typename T>
[[nodiscard]] ErrorCode ZLibDeflate(const uint8_t* uncompressed,
                                    size_t uncompressed_length, T& compressed);

template<typename T>
[[nodiscard]] ErrorCode Lz4Compress(const uint8_t* uncompressed,
                                    size_t uncompressed_length, T& compressed);

}  // namespace sdb::encoding
