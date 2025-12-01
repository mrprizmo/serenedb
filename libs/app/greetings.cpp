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

#include "greetings.h"

#include <string>

#include "app/app_server.h"
#include "basics/logger/logger.h"
#include "rest/version.h"

namespace sdb {

const char* gLgplNotice =
  "This executable uses the GNU C library (glibc), which is licensed under "
  "the GNU Lesser General Public License (LGPL), see "
  "https://www.gnu.org/copyleft/lesser.html and "
  "https://www.gnu.org/licenses/gpl.html";

void LogLgplNotice(void) {
#ifdef __GLIBC__
  SDB_INFO("xxxxx", Logger::FIXME, gLgplNotice);
#endif
}

void GreetingsFeature::prepare() {
  SDB_INFO("xxxxx", Logger::FIXME, rest::Version::getVerboseVersionString());
  LogLgplNotice();

  // building in maintainer mode or enabling unit test code will incur runtime
  // overhead, so warn users about this
#ifdef SDB_DEV
  // maintainer mode
  constexpr bool kWarn = true;
#else
  // unit tests on (enables TEST_VIRTUAL)
#ifdef SDB_GTEST
  constexpr bool kWarn = true;
#else
  // neither maintainer mode nor unit tests
  constexpr bool kWarn = false;
#endif
#endif
  // cppcheck-suppress knownConditionTrueFalse
  if (kWarn) {
    SDB_WARN("xxxxx", Logger::FIXME, "This version is FOR DEVELOPMENT ONLY!");
    SDB_WARN(
      "xxxxx", Logger::FIXME,
      "==================================================================="
      "================");
  }
}

void GreetingsFeature::unprepare() {
  SDB_INFO("xxxxx", Logger::FIXME, "SereneDB has been shut down");
}

}  // namespace sdb
