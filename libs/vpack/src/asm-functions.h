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
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>

namespace vpack {

extern size_t (*gJsonStringCopy)(uint8_t*, const uint8_t*, size_t);

// Now a version which also stops at high bit set bytes:
extern size_t (*gJsonStringCopyCheckUtf8)(uint8_t*, const uint8_t*, size_t);

// White space skipping:
extern size_t (*gJsonSkipWhiteSpace)(const uint8_t*, size_t);

// check string for invalid utf-8 sequences
extern bool (*gValidateUtf8String)(const uint8_t*, size_t);

void EnableNativeStringFunctions() noexcept;
void EnableBuiltinStringFunctions() noexcept;

}  // namespace vpack
