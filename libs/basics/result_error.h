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
#include <string_view>

#include "basics/assert.h"
#include "basics/error_code.h"
#include "basics/errors.h"

namespace sdb::result {

class Error final {
 public:
  explicit Error(ErrorCode number, std::string&& message)
    : _number{number}, _message{std::move(message)} {
    SDB_ASSERT(number != ERROR_OK);
  }

  explicit Error(ErrorCode number, std::string_view message)
    : Error{number, std::string{message}} {}

  explicit Error(ErrorCode number, const char* message)
    : Error{number, absl::NullSafeStringView(message)} {}

  explicit Error(ErrorCode number) : Error{number, std::string{}} {}

  ErrorCode errorNumber() const noexcept { return _number; }
  std::string_view errorMessage() const& noexcept;
  std::string errorMessage() && noexcept;

  bool hasErrorMessage() const noexcept { return !_message.empty(); }

  template<typename S>
  void resetErrorMessage(S&& msg) {
    _message = std::forward<S>(msg);
  }

  template<typename S>
  void appendErrorMessage(S&& msg) {
    if (_message.empty()) {
      _message += errorMessage();
    }
    _message += std::forward<S>(msg);
  }

  bool operator==(const Error&) const noexcept = default;

 private:
  ErrorCode _number = ERROR_OK;
  std::string _message;
};

}  // namespace sdb::result
