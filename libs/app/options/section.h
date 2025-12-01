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

#include <map>
#include <string>

#include "app/options/option.h"

namespace sdb::options {

// a single program options section
struct Section {
  // sections are default copy-constructible and default movable

  Section(const std::string& name, const std::string& description,
          const std::string& link, const std::string& alias, bool hidden,
          bool obsolete);

  Section(const Section&) = delete;
  Section& operator=(const Section&) = delete;
  Section(Section&&) = default;
  Section& operator=(Section&&) = default;

  ~Section();

  // get display name for the section
  std::string displayName() const;

  // whether or not the section has (displayable) options
  bool hasOptions() const;

  // print help for a section
  // the special search string "." will show help for all sections, even if
  // hidden
  void printHelp(const std::string& search, size_t tw, size_t ow,
                 bool colors) const;

  // determine display width for a section
  size_t optionsWidth() const;

  std::string name;
  std::string description;
  std::string link;
  std::string alias;
  bool hidden;
  bool obsolete;

  // program options of the section
  std::map<std::string, Option> options;

  // sub-headlines
  std::map<std::string, std::string> headlines;
};

}  // namespace sdb::options
