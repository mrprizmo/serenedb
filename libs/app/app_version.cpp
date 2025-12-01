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

#include "app/app_version.h"

#include <iostream>

#include "app/greetings.h"
#include "app/options/option.h"
#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "rest/version.h"

using namespace sdb::rest;
using namespace sdb::options;

namespace sdb {

void AppVersion::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption(
    "--version", "Print the version and other related information, then exit.",
    new BooleanParameter(&_print_version),
    options::MakeDefaultFlags(options::Flags::Command));

  options->addOption("--version-json",
                     "Print the version and other related information in JSON "
                     "format, then exit.",
                     new BooleanParameter(&_print_version_json),
                     options::MakeDefaultFlags(options::Flags::Command));
}

void AppVersion::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (_print_version_json) {
    vpack::Builder builder;
    {
      vpack::ObjectBuilder ob(&builder);
      Version::getVPack(builder);

      builder.add("version", Version::getServerVersion());
    }

    std::cout << builder.slice().toJson() << std::endl;
    exit(EXIT_SUCCESS);
  }

  if (_print_version) {
    std::cout << Version::getServerVersion() << std::endl
              << std::endl
              << gLgplNotice << std::endl
              << std::endl
              << Version::getDetailed() << std::endl;
    exit(EXIT_SUCCESS);
  }
}

}  // namespace sdb
