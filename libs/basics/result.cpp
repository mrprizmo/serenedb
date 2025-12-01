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

#include "result.h"

#include "basics/errors.h"

namespace sdb {

using result::Error;

Result::Result(ErrorCode error_number)
  : _error{error_number == ERROR_OK ? nullptr
                                    : std::make_unique<Error>(error_number)} {}

Result::Result(ErrorCode error_number, std::string&& error_message)
  : _error{
      error_number == ERROR_OK
        ? nullptr
        : std::make_unique<Error>(error_number, std::move(error_message))} {
  SDB_ASSERT(error_number != ERROR_OK || error_message.empty());
}

Result::Result(ErrorCode error_number, std::string_view error_message)
  : _error{error_number == ERROR_OK
             ? nullptr
             : std::make_unique<Error>(error_number, error_message)} {
  SDB_ASSERT(error_number != ERROR_OK || error_message.empty());
}

Result::Result(ErrorCode error_number, const char* error_message)
  : _error{error_number == ERROR_OK
             ? nullptr
             : std::make_unique<Error>(error_number, error_message)} {
  SDB_ASSERT(error_number != ERROR_OK ||
             absl::NullSafeStringView(error_message).empty());
}

Result Result::clone() const {
  Result r;
  r._error = _error ? std::make_unique<Error>(*_error) : nullptr;
  return r;
}

ErrorCode Result::errorNumber() const noexcept {
  return _error ? _error->errorNumber() : ERROR_OK;
}

void Result::reset(ErrorCode error_number) {
  if (error_number == ERROR_OK) {
    _error = nullptr;
  } else {
    _error = std::make_unique<Error>(error_number);
  }
}

void Result::reset(ErrorCode error_number, std::string_view error_message) {
  if (error_number == ERROR_OK) {
    SDB_ASSERT(error_message.empty());
    _error = nullptr;
  } else {
    _error = std::make_unique<Error>(error_number, error_message);
  }
}

void Result::reset(ErrorCode error_number, std::string&& error_message) {
  if (error_number == ERROR_OK) {
    SDB_ASSERT(error_message.empty());
    _error = nullptr;
  } else {
    _error = std::make_unique<Error>(error_number, std::move(error_message));
  }
}

std::string_view Result::errorMessage() const& noexcept {
  return _error ? _error->errorMessage() : "";
}

std::string Result::errorMessage() && noexcept {
  return _error ? std::move(*_error).errorMessage() : std::string{};
}

bool Result::operator==(const Result& other) const {
  if (!_error || !other._error) {
    return _error == other._error;
  }
  return *_error == *other._error;
}

}  // namespace sdb
