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

#include "dump_limits_feature.h"

#include "app/app_server.h"
#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "app/options/section.h"
#include "basics/application-exit.h"
#include "basics/logger/logger.h"
#include "basics/physical_memory.h"

using namespace sdb;
using namespace sdb::app;
using namespace sdb::options;

namespace {
uint64_t DefaultMemoryUsage() {
  if (physical_memory::GetValue() >= (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.2
    return static_cast<uint64_t>(
      (physical_memory::GetValue() - (static_cast<uint64_t>(2) << 30)) * 0.2);
  }
  // if we have at least 2GB of RAM, the default size is 64MB
  return (static_cast<uint64_t>(64) << 20);
}
}  // namespace

namespace sdb {

DumpLimitsFeature::DumpLimitsFeature(Server& server)
  : SerenedFeature{server, name()} {
  setOptional(false);

  _dump_limits.memory_usage = DefaultMemoryUsage();
}

void DumpLimitsFeature::collectOptions(
  std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("dump", "Dump limits");

  options
    ->addOption(
      "--dump.max-memory-usage",
      "Maximum memory usage (in bytes) to be used by all ongoing dumps.",
      new UInt64Parameter(&_dump_limits.memory_usage, 1,
                          /*minimum*/ 16 * 1024 * 1024),
      sdb::options::MakeFlags(
        sdb::options::Flags::Dynamic, sdb::options::Flags::DefaultNoComponents,
        sdb::options::Flags::OnDBServer, sdb::options::Flags::OnSingle))

    .setLongDescription(
      R"(The approximate per-server maximum allowed memory usage value
for all ongoing dump actions combined.)");

  options
    ->addOption(
      "--dump.max-docs-per-batch",
      "Maximum number of documents per batch that can be used in a dump.",
      new UInt64Parameter(&_dump_limits.docs_per_batch_upper_bound, 1,
                          /*minimum*/ _dump_limits.docs_per_batch_lower_bound),
      sdb::options::MakeFlags(
        sdb::options::Flags::Uncommon, sdb::options::Flags::DefaultNoComponents,
        sdb::options::Flags::OnDBServer, sdb::options::Flags::OnSingle))

    .setLongDescription(
      R"(Each batch in a dump can grow to at most this size.)");

  options
    ->addOption(
      "--dump.max-batch-size",
      "Maximum batch size value (in bytes) that can be used in a dump.",
      new UInt64Parameter(&_dump_limits.batch_size_upper_bound, 1,
                          /*minimum*/ _dump_limits.batch_size_lower_bound),
      sdb::options::MakeFlags(
        sdb::options::Flags::Uncommon, sdb::options::Flags::DefaultNoComponents,
        sdb::options::Flags::OnDBServer, sdb::options::Flags::OnSingle))

    .setLongDescription(
      R"(Each batch in a dump can grow to at most this size.)");

  options
    ->addOption(
      "--dump.max-parallelism",
      "Maximum parallelism that can be used in a dump.",
      new UInt64Parameter(&_dump_limits.parallelism_upper_bound, 1,
                          /*minimum*/ _dump_limits.parallelism_lower_bound),
      sdb::options::MakeFlags(
        sdb::options::Flags::Uncommon, sdb::options::Flags::DefaultNoComponents,
        sdb::options::Flags::OnDBServer, sdb::options::Flags::OnSingle))

    .setLongDescription(R"(Each dump action on a server can use at most
this many parallel threads. Note that end users can still start multiple
dump actions that run in parallel.)");
}

void DumpLimitsFeature::validateOptions(
  std::shared_ptr<options::ProgramOptions> options) {
  if (_dump_limits.batch_size_lower_bound >
      _dump_limits.batch_size_upper_bound) {
    SDB_FATAL("xxxxx", sdb::Logger::CONFIG,
              "invalid value for --dump.max-batch-size. Please use a value ",
              "of at least ", _dump_limits.batch_size_lower_bound);
  }

  if (_dump_limits.parallelism_lower_bound >
      _dump_limits.parallelism_upper_bound) {
    SDB_FATAL("xxxxx", sdb::Logger::CONFIG,
              "invalid value for --dump.max-parallelism. Please use a value ",
              "of at least ", _dump_limits.parallelism_lower_bound);
  }
}

}  // namespace sdb
