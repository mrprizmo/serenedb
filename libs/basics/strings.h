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

#include <cstdint>
#include <cstdlib>
#include <string>

namespace sdb {

char* DuplicateString(const char*, size_t length);

// Copy string of maximal length. Always append a '\0'.
void CopyString(char* dst, const char* src, size_t length);

void FreeString(char*) noexcept;

// This method decodes a UTF-8 character string by replacing the \\uXXXX
// sequence by unicode characters and representing them as UTF-8 sequences.
char* UnescapeUtf8String(const char* in, size_t in_length, size_t* out_length,
                         bool normalize);

// the buffer must be big enough to hold at least inLength + 1 bytes of chars
// returns the length of the unescaped string, excluding the trailing null
// byte
size_t UnescapeUtf8StringInPlace(char* buffer, const char* in,
                                 size_t in_length);

// determine the number of characters in a UTF-8 string
size_t CharLengthUtf8String(const char*, size_t);

std::string StringTimeStamp(double stamp, bool use_local_time);

size_t StringUInt32HexInPlace(uint32_t attr, char* buffer);

size_t StringUInt64HexInPlace(uint64_t attr, char* buffer);

}  // namespace sdb
