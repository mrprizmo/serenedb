////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include <exception>
#include <iosfwd>

#include "vpack/common.h"

namespace vpack {

class Exception : public virtual std::exception {
 public:
  enum ExceptionType : uint8_t {
    kUnknownError = 0,
    kInternalError,

    kNoJsonEquivalent,
    kParseError,
    kUnexpectedControlCharacter,
    kIndexOutOfBounds,
    kNumberOutOfRange,
    kInvalidUtf8Sequence,
    kInvalidAttributePath,
    kInvalidValueType,
    kDuplicateAttributeName,
    kBadTupleSize,
    kTooDeepNesting,

    kBuilderNotSealed,
    kBuilderNeedOpenObject,
    kBuilderNeedOpenArray,
    kBuilderNeedOpenCompound,
    kBuilderUnexpectedType,
    kBuilderUnexpectedValue,
    kBuilderNeedSubvalue,
    kBuilderKeyAlreadyWritten,
    kBuilderKeyMustBeString,

    kValidatorInvalidLength,
    kValidatorInvalidType,
  };

 private:
  ExceptionType _type;
  const char* _msg;

 public:
  Exception(ExceptionType type, const char* msg) noexcept;

  explicit Exception(ExceptionType type) noexcept
    : Exception(type, message(type)) {}

  Exception(const Exception& other) = default;
  Exception(Exception&& other) noexcept = default;
  Exception& operator=(const Exception& other) = default;
  Exception& operator=(Exception&& other) noexcept = default;

  ~Exception() override = default;

  const char* what() const noexcept override { return _msg; }

  ExceptionType errorCode() const noexcept { return _type; }

  static const char* message(ExceptionType type) noexcept;
};

}  // namespace vpack

std::ostream& operator<<(std::ostream&, const vpack::Exception*);

std::ostream& operator<<(std::ostream&, const vpack::Exception&);
