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
///
////////////////////////////////////////////////////////////////////////////////

#include "vpack/exception.h"

#include <ostream>

#include "vpack/common.h"

using namespace vpack;

Exception::Exception(ExceptionType type, const char* msg) noexcept
  : _type(type), _msg(msg) {}

const char* Exception::message(ExceptionType type) noexcept {
  switch (type) {
    case kInternalError:
      return "Internal error";
    case kNoJsonEquivalent:
      return "Type has no equivalent in JSON";
    case kParseError:
      return "Parse error";
    case kUnexpectedControlCharacter:
      return "Unexpected control character";
    case kDuplicateAttributeName:
      return "Duplicate attribute name";
    case kIndexOutOfBounds:
      return "Index out of bounds";
    case kNumberOutOfRange:
      return "Number out of range";
    case kInvalidUtf8Sequence:
      return "Invalid UTF-8 sequence";
    case kInvalidAttributePath:
      return "Invalid attribute path";
    case kInvalidValueType:
      return "Invalid value type for operation";
    case kBadTupleSize:
      return "Array size does not match tuple size";
    case kTooDeepNesting:
      return "Too deep nesting in Array/Object";
    case kBuilderNotSealed:
      return "Builder value not yet sealed";
    case kBuilderNeedOpenObject:
      return "Need open Object";
    case kBuilderNeedOpenArray:
      return "Need open Array";
    case kBuilderNeedSubvalue:
      return "Need subvalue in current Object or Array";
    case kBuilderNeedOpenCompound:
      return "Need open compound value (Array or Object)";
    case kBuilderUnexpectedType:
      return "Unexpected type";
    case kBuilderUnexpectedValue:
      return "Unexpected value";
    case kBuilderKeyAlreadyWritten:
      return "The key of the next key/value pair is already written";
    case kBuilderKeyMustBeString:
      return "The key of the next key/value pair must be a string";
    case kValidatorInvalidType:
      return "Invalid type found in binary data";
    case kValidatorInvalidLength:
      return "Invalid length found in binary data";
    case kUnknownError:
    default:
      return "Unknown error";
  }
}

std::ostream& operator<<(std::ostream& stream, const Exception* ex) {
  stream << "[Exception " << ex->what() << "]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Exception& ex) {
  return operator<<(stream, &ex);
}
