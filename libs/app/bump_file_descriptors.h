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

#include <string>

#include "app/app_feature.h"
#include "basics/operating-system.h"

#ifdef SERENEDB_HAVE_GETRLIMIT
namespace sdb {
class LoggerFeature;

class BumpFileDescriptorsFeature : public app::AppFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "BumpFileDescriptors";
  }

  explicit BumpFileDescriptorsFeature(
    app::AppServer& server,
    std::string option_name = "--server.descriptors-minimum")
    : app::AppFeature{server, name()}, _option_name{std::move(option_name)} {
    setOptional(false);
  }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) final;
  void prepare() final;

 private:
  const std::string _option_name;
  uint64_t _descriptors_minimum = 0;
};

}  // namespace sdb
#endif
