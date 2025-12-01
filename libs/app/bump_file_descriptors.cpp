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

#include "bump_file_descriptors.h"

#include "app/app_server.h"
#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "basics/application-exit.h"
#include "basics/exitcodes.h"
#include "basics/file_descriptors.h"
#include "basics/logger/logger.h"

#ifdef SERENEDB_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <absl/strings/str_cat.h>

#include <cstdlib>
#include <cstring>
#include <string>

using namespace sdb::app;
using namespace sdb::options;

#ifdef SERENEDB_HAVE_GETRLIMIT
namespace sdb {

void BumpFileDescriptorsFeature::collectOptions(
  std::shared_ptr<ProgramOptions> options) {
  // we do this initialization here so we don't need to include
  // FileDescriptors.h in the header file.
  _descriptors_minimum = FileDescriptors::recommendedMinimum();

  options->addOption(
    _option_name,
    "The minimum number of file descriptors needed to start (0 = no "
    "minimum)",
    new UInt64Parameter(&_descriptors_minimum),
    sdb::options::MakeFlags(sdb::options::Flags::DefaultNoOs,
                            sdb::options::Flags::OsLinux,
                            sdb::options::Flags::OsMac));
}

void BumpFileDescriptorsFeature::validateOptions(
  std::shared_ptr<ProgramOptions> /*options*/) {
  if (_descriptors_minimum > 0 &&
      (_descriptors_minimum < FileDescriptors::kRequiredMinimum ||
       _descriptors_minimum > FileDescriptors::kMaximumValue)) {
    SDB_FATAL("xxxxx", Logger::STARTUP, "invalid value for ", _option_name,
              ". must be between ", FileDescriptors::kRequiredMinimum, " and ",
              FileDescriptors::kMaximumValue);
  }
}

void BumpFileDescriptorsFeature::prepare() {
  if (Result res = FileDescriptors::adjustTo(
        static_cast<FileDescriptors::ValueType>(_descriptors_minimum));
      res.fail()) {
    SDB_FATAL_EXIT_CODE("xxxxx", Logger::SYSCALL, EXIT_RESOURCES_TOO_LOW, res);
  }

  FileDescriptors current;
  if (Result res = FileDescriptors::load(current); res.fail()) {
    SDB_FATAL_EXIT_CODE("xxxxx", Logger::SYSCALL, EXIT_RESOURCES_TOO_LOW,
                        "cannot get the file descriptors limit value: ", res);
  }

  SDB_INFO("xxxxx", Logger::SYSCALL,
           "file-descriptors (nofiles) hard limit is ",
           FileDescriptors::stringify(current.hard), ", soft limit is ",
           FileDescriptors::stringify(current.soft));

  auto required =
    std::max(static_cast<FileDescriptors::ValueType>(_descriptors_minimum),
             FileDescriptors::kRequiredMinimum);

  if (current.soft < required) {
    auto message = absl::StrCat(
      "file-descriptors (nofiles) soft limit is too low, currently ",
      FileDescriptors::stringify(current.soft), ". please raise to at least ",
      required, " (e.g. via ulimit -n ", required,
      ") or adjust the value of the startup option ", _option_name);
    if (_descriptors_minimum == 0) {
      SDB_WARN("xxxxx", Logger::SYSCALL, message);
    } else {
      SDB_FATAL_EXIT_CODE("xxxxx", Logger::SYSCALL, EXIT_RESOURCES_TOO_LOW,
                          message);
    }
  }
}

}  // namespace sdb
#endif
