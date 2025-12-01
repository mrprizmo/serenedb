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

#include "app/options_check.h"

#include "app/app_server.h"
#include "app/options/option.h"
#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "basics/logger/logger.h"

using namespace sdb::options;

namespace sdb {

void OptionsCheckFeature::prepare() {
  const auto& options = server().options();

  const auto& modernized_options = options->modernizedOptions();
  if (!modernized_options.empty()) {
    for (const auto& it : modernized_options) {
      SDB_WARN("xxxxx", Logger::STARTUP,
               "please note that the specified option '--", it.first,
               " has been renamed to '--", it.second, "'");
    }

    SDB_INFO("xxxxx", Logger::STARTUP,
             "please read the release notes about changed options");
  }

  options->walk(
    [](const Section&, const Option& option) {
      if (option.hasDeprecatedIn()) {
        SDB_WARN("xxxxx", Logger::STARTUP, "option '", option.displayName(),
                 "' is deprecated since ", option.deprecatedInString(),
                 " and may be removed or unsupported in a future version");
      }
    },
    true, true);

  // inform about obsolete options
  options->walk(
    [](const Section&, const Option& option) {
      if (option.hasFlag(sdb::options::Flags::Obsolete)) {
        SDB_WARN("xxxxx", Logger::STARTUP, "obsolete option '",
                 option.displayName(), "' used in configuration. ",
                 "Setting this option does not have any effect.");
      }
    },
    true, true);

  // inform about experimental options
  options->walk(
    [](const Section&, const Option& option) {
      if (option.hasFlag(sdb::options::Flags::Experimental)) {
        SDB_WARN("xxxxx", Logger::STARTUP, "experimental option '",
                 option.displayName(), "' used in configuration.");
      }
    },
    true, true);
}

}  // namespace sdb
