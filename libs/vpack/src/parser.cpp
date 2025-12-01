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

#include "vpack/parser.h"

#include <cstdlib>

#include "asm-functions.h"
#include "vpack/common.h"
#include "vpack/value.h"
#include "vpack/value_type.h"

using namespace vpack;

// The following function does the actual parse. It gets bytes
// via peek, consume and reset appends the result to the Builder
// in *_builder. Errors are reported via an exception.
// Behind the scenes it runs two parses, one to collect sizes and
// check for parse errors (scan phase) and then one to actually
// build the result (build phase).

ValueLength Parser::parseInternal(bool multi) {
  // skip over optional BOM
  if (_size >= 3 && _start[0] == 0xef && _start[1] == 0xbb &&
      _start[2] == 0xbf) {
    // found UTF-8 BOM. simply skip over it
    _pos += 3;
  }

  ValueLength nr = 0;
  do {
    bool have_reported = false;
    if (!_builder->_stack.empty()) {
      const ValueLength tos = _builder->_stack.back().start_pos;
      if (_builder->_start[tos] == 0x0b || _builder->_start[tos] == 0x14) {
        if (!_builder->_key_written) {
          throw Exception(Exception::kBuilderKeyMustBeString);
        }
        _builder->_key_written = false;
      } else {
        _builder->reportAdd();
        have_reported = true;
      }
    }
    try {
      parseJson();
    } catch (...) {
      if (have_reported) {
        _builder->cleanupAdd();
      }
      throw;
    }
    nr++;
    while (_pos < _size && isWhiteSpace(_start[_pos])) {
      ++_pos;
    }
    if (!multi && _pos != _size) {
      consume();
      // to get error reporting right. return value intentionally not checked
      throw Exception(Exception::kParseError, "Expecting EOF");
    }
  } while (multi && _pos < _size);
  return nr;
}

// skips over all following whitespace tokens but does not consume the
// byte following the whitespace
int Parser::skipWhiteSpace(const char* err) {
  if (_pos >= _size) [[unlikely]] {
    throw Exception(Exception::kParseError, err);
  }
  uint8_t c = _start[_pos];
  if (!isWhiteSpace(c)) {
    return c;
  }
  if (c == ' ') {
    if (_pos + 1 >= _size) {
      _pos++;
      throw Exception(Exception::kParseError, err);
    }
    c = _start[_pos + 1];
    if (!isWhiteSpace(c)) {
      _pos++;
      return c;
    }
  }
  size_t remaining = _size - _pos;
  if (remaining >= 16) {
    size_t count = gJsonSkipWhiteSpace(_start + _pos, remaining - 15);
    _pos += count;
  }
  do {
    if (!isWhiteSpace(_start[_pos])) {
      return static_cast<int>(_start[_pos]);
    }
    _pos++;
  } while (_pos < _size);
  throw Exception(Exception::kParseError, err);
}

void Parser::increaseNesting() {
  if (++_nesting >= options->nesting_limit) {
    throw Exception(Exception::kTooDeepNesting);
  }
}

void Parser::decreaseNesting() noexcept {
  SDB_ASSERT(_nesting > 0);
  --_nesting;
}

// parses a number value
void Parser::parseNumber() {
  size_t start_pos = _pos;
  ParsedNumber number_value;
  bool negative = false;
  int i = consume();
  // We know that a character is coming, and it's a number if it
  // starts with '-' or a digit. otherwise it's invalid
  if (i == '-') {
    i = getOneOrThrow("Incomplete number");
    negative = true;
  }
  if (i < '0' || i > '9') {
    throw Exception(Exception::kParseError, "Expecting digit");
  }

  if (i != '0') {
    unconsume();
    scanDigits(number_value);
  }
  i = consume();
  if (i < 0 || (i != '.' && i != 'e' && i != 'E')) {
    if (i >= 0) {
      unconsume();
    }
    if (!number_value.is_integer) {
      if (negative) {
        _builder->addDouble(-number_value.double_value);
      } else {
        _builder->addDouble(number_value.double_value);
      }
    } else if (negative) {
      if (number_value.int_value < uint64_t{1} << 63) {
        _builder->addInt(-static_cast<int64_t>(number_value.int_value));
      } else if (number_value.int_value == uint64_t{1} << 63) {
        _builder->addInt(std::numeric_limits<int64_t>::min());
      } else {
        _builder->addDouble(-static_cast<double>(number_value.int_value));
      }
    } else {
      _builder->addUInt(number_value.int_value);
    }
    return;
  }

  double fractional_part;
  if (i == '.') {
    // fraction. skip over '.'
    i = getOneOrThrow("Incomplete number");
    if (i < '0' || i > '9') {
      throw Exception(Exception::kParseError, "Incomplete number");
    }
    unconsume();
    fractional_part = scanDigitsFractional();
    if (negative) {
      fractional_part = -number_value.asDouble() - fractional_part;
    } else {
      fractional_part = number_value.asDouble() + fractional_part;
    }
    i = consume();
    if (i < 0) {
      _builder->addDouble(fractional_part);
      return;
    }
  } else {
    if (negative) {
      fractional_part = -number_value.asDouble();
    } else {
      fractional_part = number_value.asDouble();
    }
  }
  if (i != 'e' && i != 'E') {
    unconsume();
    // use conventional atof() conversion here, to avoid precision loss
    // when interpreting and multiplying the single digits of the input stream
    // _builder->addDouble(fractionalPart);
    _builder->addDouble(
      atof(reinterpret_cast<const char*>(_start) + start_pos));
    return;
  }
  i = getOneOrThrow("Incomplete number");
  negative = false;
  if (i == '+' || i == '-') {
    negative = (i == '-');
    i = getOneOrThrow("Incomplete number");
  }
  if (i < '0' || i > '9') {
    throw Exception(Exception::kParseError, "Incomplete number");
  }
  unconsume();
  ParsedNumber exponent;
  scanDigits(exponent);
  if (negative) {
    fractional_part *= pow(10, -exponent.asDouble());
  } else {
    fractional_part *= pow(10, exponent.asDouble());
  }
  if (std::isnan(fractional_part) || !std::isfinite(fractional_part)) {
    throw Exception(Exception::kNumberOutOfRange);
  }
  // use conventional atof() conversion here, to avoid precision loss
  // when interpreting and multiplying the single digits of the input stream
  // _builder->addDouble(fractionalPart);
  _builder->addDouble(atof(reinterpret_cast<const char*>(_start) + start_pos));
}

void Parser::parseString() {
  // When we get here, we have seen a " character and now want to
  // find the end of the string and parse the string value to its
  // VPack representation. We assume that the string is short and
  // insert 8 bytes for the length as soon as we reach 127 bytes
  // in the VPack representation.
  const ValueLength base = _builder->_pos;
  _builder->appendByte(0x80);  // correct this later

  bool large = false;           // set to true when we reach 128 bytes
  uint32_t high_surrogate = 0;  // non-zero if high-surrogate was seen

  while (true) {
    size_t remainder = _size - _pos;
    if (remainder >= 16) {
      _builder->reserve(remainder);
      size_t count;
      // Note that the SSE4.2 accelerated string copying functions might
      // peek up to 15 bytes over the given end, because they use 128bit
      // registers. Therefore, we have to subtract 15 from remainder
      // to be on the safe side. Further bytes will be processed below.
      if (options->validate_utf8_strings) {
        count = gJsonStringCopyCheckUtf8(_builder->_start + _builder->_pos,
                                         _start + _pos, remainder - 15);
      } else {
        count = gJsonStringCopy(_builder->_start + _builder->_pos,
                                _start + _pos, remainder - 15);
      }
      _pos += count;
      _builder->advance(count);
    }
    int i = getOneOrThrow("Unfinished string");
    if (!large && _builder->_pos - (base + 1) > 126) {
      large = true;
      _builder->reserve(4);
      auto len = _builder->_pos - (base + 1);
      memmove(_builder->_start + base + 5, _builder->_start + base + 1, len);
      _builder->advance(4);
    }

    switch (i) {
      case '"':
        if (!large) {
          auto len = _builder->_pos - (base + 1);
          _builder->_start[base] = 0x80 + static_cast<uint8_t>(len);
        } else {
          auto len = _builder->_pos - (base + 5);
          if (len > std::numeric_limits<uint32_t>::max()) [[unlikely]] {
            throw Exception{Exception::kInternalError,
                            "vpack::Parser is unable to allocate string"};
          }
          _builder->_start[base] = 0xff;
          absl::little_endian::Store32(_builder->_start + (base + 1), len);
        }
        if (options->validate_utf8_strings && high_surrogate != 0)
          [[unlikely]] {
          throw Exception(Exception::kInvalidUtf8Sequence,
                          "Unexpected end of string after high surrogate");
        }
        return;
      case '\\':
        // Handle cases or throw error
        i = consume();
        if (i < 0) [[unlikely]] {
          throw Exception(Exception::kParseError, "Invalid escape sequence");
        }
        switch (i) {
          case '"':
          case '/':
          case '\\':
            _builder->appendByte(static_cast<uint8_t>(i));
            break;
          case 'b':
            _builder->appendByte('\b');
            break;
          case 'f':
            _builder->appendByte('\f');
            break;
          case 'n':
            _builder->appendByte('\n');
            break;
          case 'r':
            _builder->appendByte('\r');
            break;
          case 't':
            _builder->appendByte('\t');
            break;
          case 'u': {
            uint32_t v = 0;
            for (int j = 0; j < 4; j++) {
              i = consume();
              if (i < 0) {
                throw Exception(Exception::kParseError,
                                "Unfinished \\uXXXX escape sequence");
              }
              if (i >= '0' && i <= '9') {
                v = (v << 4) + i - '0';
              } else if (i >= 'a' && i <= 'f') {
                v = (v << 4) + i - 'a' + 10;
              } else if (i >= 'A' && i <= 'F') {
                v = (v << 4) + i - 'A' + 10;
              } else {
                throw Exception(Exception::kParseError,
                                "Illegal \\uXXXX escape sequence character");
              }
            }
            if (v < 0x80) {
              _builder->appendByte(static_cast<uint8_t>(v));
            } else if (v < 0x800) {
              _builder->reserve(2);
              _builder->appendByteUnchecked(0xc0 + (v >> 6));
              _builder->appendByteUnchecked(0x80 + (v & 0x3f));
            } else if (v >= 0xdc00 && v < 0xe000) {
              if (high_surrogate != 0) {
                // Low surrogate, put the two together:
                v = 0x10000 + ((high_surrogate - 0xd800) << 10) + v - 0xdc00;
                _builder->rollback(3);
                _builder->reserve(4);
                _builder->appendByteUnchecked(0xf0 + (v >> 18));
                _builder->appendByteUnchecked(0x80 + ((v >> 12) & 0x3f));
                _builder->appendByteUnchecked(0x80 + ((v >> 6) & 0x3f));
                _builder->appendByteUnchecked(0x80 + (v & 0x3f));
                high_surrogate = 0;
              } else if (options->validate_utf8_strings) {
                // Low surrogate without a high surrogate first
                throw Exception(Exception::kInvalidUtf8Sequence,
                                "Unexpected \\uXXXX escape sequence (low "
                                "surrogate without high surrogate)");
              }
            } else if (v >= 0xd800 && v < 0xdc00) {
              if (high_surrogate == 0) {
                // High surrogate:
                high_surrogate = v;
                _builder->reserve(3);
                _builder->appendByteUnchecked(0xe0 + (v >> 12));
                _builder->appendByteUnchecked(0x80 + ((v >> 6) & 0x3f));
                _builder->appendByteUnchecked(0x80 + (v & 0x3f));

                continue;
              } else if (options->validate_utf8_strings) {
                throw Exception(Exception::kInvalidUtf8Sequence,
                                "Unexpected \\uXXXX escape sequence (multiple "
                                "adjacent high surrogates)");
              }
            } else {
              _builder->reserve(3);
              _builder->appendByteUnchecked(0xe0 + (v >> 12));
              _builder->appendByteUnchecked(0x80 + ((v >> 6) & 0x3f));
              _builder->appendByteUnchecked(0x80 + (v & 0x3f));
            }
            break;
          }
          default:
            throw Exception(Exception::kParseError, "Invalid escape sequence");
        }
        break;
      default:
        if ((i & 0x80) == 0) {
          // non-UTF-8 sequence
          if (i < 0x20) [[unlikely]] {
            // control character
            throw Exception(Exception::kUnexpectedControlCharacter);
          }
          _builder->appendByte(static_cast<uint8_t>(i));
        } else {
          if (!options->validate_utf8_strings) {
            _builder->appendByte(static_cast<uint8_t>(i));
          } else {
            // multi-byte UTF-8 sequence!
            int follow = 0;
            if ((i & 0xe0) == 0x80) {
              throw Exception(Exception::kInvalidUtf8Sequence);
            } else if ((i & 0xe0) == 0xc0) {
              // two-byte sequence
              follow = 1;
            } else if ((i & 0xf0) == 0xe0) {
              // three-byte sequence
              follow = 2;
            } else if ((i & 0xf8) == 0xf0) {
              // four-byte sequence
              follow = 3;
            } else {
              throw Exception(Exception::kInvalidUtf8Sequence);
            }

            // validate follow up characters
            _builder->reserve(1 + follow);
            _builder->appendByteUnchecked(static_cast<uint8_t>(i));
            for (int j = 0; j < follow; ++j) {
              i = getOneOrThrow("scanString: truncated UTF-8 sequence");
              if ((i & 0xc0) != 0x80) {
                throw Exception(Exception::kInvalidUtf8Sequence);
              }
              _builder->appendByteUnchecked(static_cast<uint8_t>(i));
            }
          }
        }
        break;
    }

    if (options->validate_utf8_strings && high_surrogate != 0) [[unlikely]] {
      throw Exception(Exception::kInvalidUtf8Sequence,
                      "Unexpected \\uXXXX escape sequence (high surrogate "
                      "without low surrogate)");
    }
  }
}

void Parser::parseArray() {
  _builder->addArray();

  increaseNesting();

  int i = skipWhiteSpace("Expecting item or ']'");
  if (i == ']') {
    // empty array
    ++_pos;  // the closing ']'
    decreaseNesting();
    _builder->close();
    return;
  }

  while (true) {
    // parse array element itself
    _builder->reportAdd();
    parseJson();
    i = skipWhiteSpace("Expecting ',' or ']'");
    if (i == ']') {
      // end of array
      ++_pos;  // the closing ']'
      _builder->close();
      decreaseNesting();
      return;
    }
    // skip over ','
    if (i != ',') [[unlikely]] {
      throw Exception(Exception::kParseError, "Expecting ',' or ']'");
    }
    ++_pos;  // the ','
  }

  // should never get here
  SDB_ASSERT(false);
}

void Parser::parseObject() {
  _builder->addObject();

  increaseNesting();
  int i = skipWhiteSpace("Expecting item or '}'");
  if (i == '}') {
    // empty object
    consume();  // the closing '}'. return value intentionally not checked

    if (_nesting != 0 || !options->keep_top_level_open) {
      // only close if we've not been asked to keep top level open
      decreaseNesting();
      _builder->close();
    }
    return;
  }

  while (true) {
    // always expecting a string attribute name here
    if (i != '"') [[unlikely]] {
      throw Exception(Exception::kParseError, "Expecting '\"' or '}'");
    }
    // get past the initial '"'
    ++_pos;

    _builder->reportAdd();
    parseString();

    i = skipWhiteSpace("Expecting ':'");
    // always expecting the ':' here
    if (i != ':') [[unlikely]] {
      throw Exception(Exception::kParseError, "Expecting ':'");
    }
    ++_pos;  // skip over the colon

    parseJson();

    i = skipWhiteSpace("Expecting ',' or '}'");
    if (i == '}') {
      // end of object
      ++_pos;  // the closing '}'
      if (_nesting != 1 || !options->keep_top_level_open) {
        // only close if we've not been asked to keep top level open
        _builder->close();
      }
      decreaseNesting();
      return;
    }
    if (i != ',') [[unlikely]] {
      throw Exception(Exception::kParseError, "Expecting ',' or '}'");
    }
    // skip over ','
    ++_pos;  // the ','
    i = skipWhiteSpace("Expecting '\"' or '}'");
  }

  // should never get here
  SDB_ASSERT(false);
}

void Parser::parseJson() {
  skipWhiteSpace("Expecting item");  // return value intentionally not checked

  int i = consume();
  if (i < 0) {
    return;
  }
  switch (i) {
    case '{':
      parseObject();  // this consumes the closing '}' or throws
      break;
    case '[':
      parseArray();  // this consumes the closing ']' or throws
      break;
    case 't':
      parseTrue();  // this consumes "rue" or throws
      break;
    case 'f':
      parseFalse();  // this consumes "alse" or throws
      break;
    case 'n':
      parseNull();  // this consumes "ull" or throws
      break;
    case '"':
      parseString();
      break;
    default: {
      // everything else must be a number or is invalid...
      // this includes '-' and '0' to '9'. scanNumber() will
      // throw if the input is non-numeric
      unconsume();
      parseNumber();  // this consumes the number or throws
      break;
    }
  }
}
