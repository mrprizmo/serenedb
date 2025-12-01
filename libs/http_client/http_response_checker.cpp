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

#include "http_response_checker.h"

#include <absl/strings/str_cat.h>
#include <vpack/builder.h>
#include <vpack/iterator.h>
#include <vpack/parser.h>

#include "basics/static_strings.h"

using namespace sdb;

/// Trim the payload so it doesn't have the password field with the
/// password in plain text
void HttpResponseChecker::trimPayload(vpack::Slice input,
                                      vpack::Builder& output) {
  using namespace vpack;
  if (input.isObject()) {
    output.openObject();
    ObjectIterator it(input);
    while (it.valid()) {
      // trim passwd key from output.
      auto kv = *it;
      if (kv.key.stringView() != "passwd") {
        output.add(kv.key);
        trimPayload(kv.value(), output);
      }
      it.next();
    }
    output.close();
  } else if (input.isArray()) {
    output.openArray();
    vpack::ArrayIterator it(input);
    while (it.valid()) {
      trimPayload(it.value(), output);
      it.next();
    }
    output.close();
  } else {
    output.add(input);
  }
}

/// Check for error in http response
Result HttpResponseChecker::check(const std::string& client_error_msg,
                                  const httpclient::HttpResult* response,
                                  const std::string& action_msg,
                                  std::string_view request_payload,
                                  PayloadType type) {
  if (response != nullptr && !response->wasHttpError() &&
      response->isComplete()) {
    return {};
  }
  // here we will trim the payload to remove every field that contains the
  // password in plain text, e.g. "passwd" field, and we don't have a way to
  // trim it with vpack, so we have to parse the JSON and rebuild it
  // without the "passwd" fields.
  std::string msg_body;
  if (!request_payload.empty()) {
    vpack::Builder output;
    if (type == PayloadType::JSON) {
      try {
        auto payload_vpack = vpack::Parser::fromJson(request_payload.data(),
                                                     request_payload.size());
        trimPayload(payload_vpack->slice(), output);
        msg_body = output.toJson();
      } catch (...) {
        msg_body = request_payload;
      }
    } else if (type == PayloadType::VPACK) {
      trimPayload(
        vpack::Slice(reinterpret_cast<const uint8_t*>(request_payload.data())),
        output);
      msg_body = output.toJson();
    } else {
      // probably JSONL. use as is
      msg_body = request_payload;
    }
  }
  constexpr size_t kMaxMsgBodySize = 4096;  // max amount of bytes in msg body
                                            // to truncate if too big to display
  if (msg_body.size() > kMaxMsgBodySize) {  // truncate error message
    msg_body.resize(kMaxMsgBodySize);
    msg_body.append("...");
  }
  if (response == nullptr || !response->isComplete()) {
    return {
      ERROR_INTERNAL,
      absl::StrCat(
        "got invalid response from server ",
        (client_error_msg.empty() ? "" : ": '" + client_error_msg + "'"),
        (action_msg.empty() ? "" : " while executing " + action_msg),
        (msg_body.empty() ? ""
                          : " with this request payload: '" + msg_body + "'"))};
  }
  auto error_num = static_cast<int>(ERROR_INTERNAL);
  std::string error_msg = response->getHttpReturnMessage();
  try {
    std::shared_ptr<vpack::Builder> body_builder(response->getBodyVPack());
    vpack::Slice error = body_builder->slice();
    if (!error.isNone() && error.hasKey(StaticStrings::kErrorMessage)) {
      error_num = error.get(StaticStrings::kErrorNum).getNumber<int>();
      error_msg = error.get(StaticStrings::kErrorMessage).stringView();
    }
  } catch (...) {
    error_msg = response->getHttpReturnMessage();
    error_num = response->getHttpReturnCode();
  }

  auto err = ErrorCode{error_num};
  return {err, absl::StrCat(
                 "got invalid response from server: HTTP ",
                 response->getHttpReturnCode(), ": '", error_msg, "'",
                 (action_msg.empty() ? "" : " while executing " + action_msg),
                 (msg_body.empty()
                    ? ""
                    : " with this request payload: '" + msg_body + "'"))};
}

/// Check for error in http response
Result HttpResponseChecker::check(
  const std::string& client_error_msg,
  const httpclient::HttpResult* const response) {
  return check(client_error_msg, response, "", "", PayloadType::JSON);
}
