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

#include "app/temp_path.h"

#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "app/options/section.h"
#include "basics/file_utils.h"
#include "basics/files.h"
#include "basics/logger/logger.h"
#include "basics/string_utils.h"
#include "basics/thread.h"

using namespace sdb::options;

namespace sdb {

void TempPath::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("temp", "temporary files");

  options
    ->addOption("--temp.path", "The path for temporary files.",
                new StringParameter(&_path))
    .setLongDescription(R"(SereneDB uses the path for storing temporary
files, for extracting data from uploaded zip file, and other things.

Ideally, the temporary path is set to an instance-specific subdirectory of the
operating system's temporary directory. To avoid data loss, the temporary path
should not overlap with any directories that contain important data, for
example, the instance's database directory.

If you set the temporary path to the same directory as the instance's database
directory, a startup error is logged and the startup is aborted.)");
}

void TempPath::validateOptions(std::shared_ptr<ProgramOptions> /*options*/) {
  if (!_path.empty()) {
    // replace $PID in basepath with current process id
    _path = basics::string_utils::Replace(
      _path, "$PID", std::to_string(Thread::currentProcessId()));

    basics::file_utils::MakePathAbsolute(_path);
  }
}

void TempPath::prepare() {
  SdbSetApplicationName(_appname);
  if (!_path.empty()) {
    SdbSetTempPath(_path);
  }
}

}  // namespace sdb
