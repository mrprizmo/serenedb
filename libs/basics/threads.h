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

#include "basics/assert.h"
#include "basics/error_code.h"
#include "basics/operating-system.h"

#define SERENEDB_INFINITE 0xFFFFFFFF  // Infinite timeout

namespace sdb {

void SdbInitThread(pthread_t* thread);

bool SdbStartThread(pthread_t*, const char*, void (*starter)(void*),
                    void* data);

bool SdbIsSelfThread(pthread_t* thread);

ErrorCode SdbJoinThread(pthread_t* thread);

ErrorCode SdbJoinThreadWithTimeout(pthread_t* thread, uint32_t timeout);

bool SdbDetachThread(pthread_t* thread);

}  // namespace sdb
