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

#include "ini_file_parser.h"

#include <cstddef>
#include <map>
#include <sstream>

#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "basics/application-exit.h"
#include "basics/common.h"
#include "basics/exceptions.h"
#include "basics/exitcodes.h"
#include "basics/file_utils.h"
#include "basics/logger/logger.h"
#include "basics/string_utils.h"

namespace sdb::options {

IniFileParser::IniFileParser(ProgramOptions* options) : _options(options) {
  // a line with just comments, e.g. #... or ;...
  _matchers.comment = std::regex("^[ \t]*([#;].*)?$",
                                 std::regex::nosubs | std::regex::ECMAScript);
  // a line that starts a section, e.g. [server]
  _matchers.section =
    std::regex("^[ \t]*\\[([-_A-Za-z0-9]*)\\][ \t]*$", std::regex::ECMAScript);
  // a line that assigns a value to a named variable
  _matchers.assignment = std::regex(
    "^[ \t]*(([-_A-Za-z0-9]*\\.)?[-_A-Za-z0-9]*)[ \t]*=[ \t]*(.*?)?[ \t]*$",
    std::regex::ECMAScript);
  // an include line
  _matchers.include = std::regex(
    "^[ \t]*@include[ \t]*([-_A-Za-z0-9/\\.]*)[ \t]*$", std::regex::ECMAScript);
}

// parse a config file. returns true if all is well, false otherwise
// errors that occur during parse are reported to _options
bool IniFileParser::parse(const std::string& filename,
                          bool end_pass_afterwards) {
  if (filename.empty()) {
    _options->fail(
      EXIT_CONFIG_NOT_FOUND,
      "unable to open configuration file: no configuration file specified");
    return false;
  }

  std::string buf;
  try {
    buf = sdb::basics::file_utils::Slurp(filename);
  } catch (const sdb::basics::Exception& ex) {
    _options->fail(EXIT_CONFIG_NOT_FOUND,
                   std::string("Couldn't open configuration file: '") +
                     filename + "' - " + ex.what());
    return false;
  }

  return parseContent(filename, buf, end_pass_afterwards);
}

// parse a config file, with the contents already read into <buf>.
// returns true if all is well, false otherwise
// errors that occur during parse are reported to _options
bool IniFileParser::parseContent(const std::string& filename,
                                 const std::string& buf,
                                 bool end_pass_afterwards) {
  std::string current_section;
  size_t line_number = 0;

  std::istringstream iss(buf);
  for (std::string line; std::getline(iss, line);) {
    basics::string_utils::TrimInPlace(line);
    ++line_number;

    if (std::regex_match(line, _matchers.comment)) {
      // skip over comments
      continue;
    }

    // set context for parsing (used in error messages)
    _options->setContext("config file '" + filename + "', line #" +
                         std::to_string(line_number));

    std::smatch match;
    if (std::regex_match(line, match, _matchers.section)) {
      // found section
      current_section = match[1].str();
    } else if (std::regex_match(line, match, _matchers.include)) {
      // found include
      std::string include(match[1].str());

      if (!include.ends_with(".conf")) {
        include += ".conf";
      }
      if (_seen.find(include) != _seen.end()) {
        SDB_FATAL_EXIT_CODE("xxxxx", Logger::CONFIG, EXIT_CONFIG_NOT_FOUND,
                            "recursive include of file '", include, "'");
      }

      _seen.insert(include);

      if (!basics::file_utils::IsRegularFile(include)) {
        auto dn = basics::file_utils::Dirname(filename);
        include = basics::file_utils::BuildFilename(dn, include);
      }

      SDB_DEBUG("xxxxx", Logger::CONFIG, "reading include file '", include,
                "'");

      if (!parse(include, false)) {
        return false;
      }
    } else if (std::regex_match(line, match, _matchers.assignment)) {
      // found assignment
      std::string option;
      std::string value(match[3].str());

      if (current_section.empty() || !match[2].str().empty()) {
        // use option as specified
        option = match[1].str();
      } else {
        // use option prefixed with current section
        option = current_section + "." + match[1].str();
      }

      if (!_options->setValue(option, value)) {
        return false;
      }
    } else {
      // unknown type of line. cannot handle it
      _options->fail(EXIT_CONFIG_NOT_FOUND,
                     "unknown line type in file '" + filename + "', line " +
                       std::to_string(line_number) + ": '" + line + "'");
      return false;
    }
  }

  // all is well
  if (end_pass_afterwards) {
    _options->endPass();
  }
  return true;
}

}  // namespace sdb::options
