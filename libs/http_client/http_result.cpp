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

#include "http_result.h"

#include <vpack/parser.h>
#include <vpack/vpack_helper.h>

#include <string>
#include <string_view>

#include "basics/encoding_utils.h"
#include "basics/exceptions.h"
#include "basics/number_utils.h"
#include "basics/static_strings.h"
#include "basics/string_utils.h"

using namespace sdb;
using namespace sdb::basics;

namespace sdb::httpclient {

HttpResult::HttpResult()
  : _return_message(),
    _content_length(0),
    _return_code(0),
    _encoding_type(rest::EncodingType::Unset),
    _found_header(false),
    _has_content_length(false),
    _chunked(false),
    _request_result_type(ResultType::Unknown) {}

HttpResult::~HttpResult() = default;

void HttpResult::clear() {
  _return_message.clear();
  _content_length = 0;
  _return_code = 0;
  _encoding_type = rest::EncodingType::Unset;
  _found_header = false;
  _has_content_length = false;
  _chunked = false;
  _request_result_type = ResultType::Unknown;
  _header_fields.clear();
  _result_body.clear();
}

StringBuffer& HttpResult::getBody() { return _result_body; }

const StringBuffer& HttpResult::getBody() const { return _result_body; }

std::shared_ptr<vpack::Builder> HttpResult::getBodyVPack() const {
  std::string_view data{_result_body.Impl()};
  vpack::Parser parser(&VPackHelper::gLooseRequestValidationOptions);
  parser.parse(data);
  return parser.steal();
}

void HttpResult::addHeaderField(const char* line, size_t length) {
  auto find = static_cast<const char*>(
    memchr(static_cast<const void*>(line), ':', length));

  if (find == nullptr) {
    find = static_cast<const char*>(
      memchr(static_cast<const void*>(line), ' ', length));
  }

  if (find != nullptr) {
    size_t l = find - line;
    addHeaderField({line, l}, {find + 1, length - l - 1});
  }
}

void HttpResult::addHeaderField(std::string_view key_str,
                                std::string_view value_str) {
  auto* key = key_str.data();
  auto key_length = key_str.size();
  // trim key
  {
    const char* end = key + key_length;

    while (key < end && (*key == ' ' || *key == '\t')) {
      ++key;
      --key_length;
    }
  }

  auto key_string = absl::AsciiStrToLower(std::string_view{key, key_length});

  auto* value = value_str.data();
  auto value_length = value_str.size();
  // trim value
  {
    const char* end = value + value_length;

    while (value < end && (*value == ' ' || *value == '\t')) {
      ++value;
      --value_length;
    }
  }

  if (!_found_header &&
      (key_string == "http/1.1" || key_string == "http/1.0") &&
      value_length > 2) {
    _found_header = true;

    // we assume the status code is 3 chars long
    if ((value[0] >= '0' && value[0] <= '9') &&
        (value[1] >= '0' && value[1] <= '9') &&
        (value[2] >= '0' && value[2] <= '9')) {
      // set response code
      setHttpReturnCode(100 * (value[0] - '0') + 10 * (value[1] - '0') +
                        (value[2] - '0'));

      if (_return_code == 204) {
        // HTTP 204 = No content. Assume we will have a content-length of 0.
        // note that the value can be overridden later if the response has
        // the content-length header set to some other value
        setContentLength(0);
      }
    }

    if (value_length >= 4) {
      setHttpReturnMessage(std::string(value + 4, value_length - 4));
    }
  } else if (key_string == StaticStrings::kContentLength) {
    setContentLength(
      number_utils::AtoiZero<size_t>(value, value + value_length));
  } else if (key_string == StaticStrings::kContentEncoding) {
    if (std::string_view(value, value_length) ==
        StaticStrings::kEncodingDeflate) {
      _encoding_type = rest::EncodingType::Deflate;
    } else if (std::string_view(value, value_length) ==
               StaticStrings::kEncodingGzip) {
      _encoding_type = rest::EncodingType::GZip;
    } else if (std::string_view(value, value_length) ==
               StaticStrings::kEncodingSereneLz4) {
      _encoding_type = rest::EncodingType::Lz4;
    }
  } else if (key_string == StaticStrings::kTransferEncoding &&
             std::string_view(value, value_length) == StaticStrings::kChunked) {
    _chunked = true;
  }

  _header_fields[std::move(key_string)] = std::string(value, value_length);
}

std::string HttpResult::getHeaderField(std::string_view name,
                                       bool& found) const {
  auto find = _header_fields.find(name);

  if (find == _header_fields.end()) {
    found = false;
    return {};
  }

  found = true;
  return find->second;
}

bool HttpResult::hasHeaderField(std::string_view name) const {
  return _header_fields.contains(name);
}

}  // namespace sdb::httpclient
