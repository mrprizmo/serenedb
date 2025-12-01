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

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "vpack/builder.h"
#include "vpack/common.h"
#include "vpack/exception.h"
#include "vpack/options.h"

namespace vpack {

class Parser {
  // This class can parse JSON very rapidly, but only from contiguous
  // blocks of memory. It builds the result using the Builder.

  struct ParsedNumber {
    ParsedNumber() : int_value(0), double_value(0.0), is_integer(true) {}

    void addDigit(int i) {
      if (is_integer) {
        // check if adding another digit to the int will make it overflow
        if (int_value < 1844674407370955161ULL ||
            (int_value == 1844674407370955161ULL && (i - '0') <= 5)) {
          // int won't overflow
          int_value = int_value * 10 + (i - '0');
          return;
        }
        // int would overflow
        double_value = static_cast<double>(int_value);
        is_integer = false;
      }

      double_value = double_value * 10.0 + (i - '0');
      if (std::isnan(double_value) || !std::isfinite(double_value)) {
        throw Exception(Exception::kNumberOutOfRange);
      }
    }

    double asDouble() const {
      if (is_integer) {
        return static_cast<double>(int_value);
      }
      return double_value;
    }

    uint64_t int_value;
    double double_value;
    bool is_integer;
  };

  std::shared_ptr<Builder> _builder;
  const uint8_t* _start = nullptr;
  size_t _size = 0;
  size_t _pos = 0;
  uint32_t _nesting = 0;

 public:
  const Options* options;

  Parser(const Parser&) = delete;
  Parser(Parser&&) = delete;
  Parser& operator=(const Parser&) = delete;
  Parser& operator=(Parser&&) = delete;
  ~Parser() = default;

  explicit Parser(const Options* options = &Options::gDefaults)
    : _builder{std::make_shared<Builder>(options)}, options{options} {}

  explicit Parser(std::shared_ptr<Builder> builder,
                  const Options* options = &Options::gDefaults)
    : _builder{std::move(builder)}, options{options} {
    if (!options) [[unlikely]] {
      throw Exception{Exception::kInternalError, "Options cannot be a nullptr"};
    }
  }

  // This method produces a parser that does not own the builder
  explicit Parser(Builder& builder,
                  const Options* options = &Options::gDefaults)
    : _builder{std::shared_ptr<Builder>{}, &builder}, options{options} {
    if (!options) [[unlikely]] {
      throw Exception{Exception::kInternalError, "Options cannot be a nullptr"};
    }
  }

  const Builder& builder() const noexcept {
    SDB_ASSERT(_builder);
    return *_builder;
  }

  static std::shared_ptr<Builder> fromJson(
    std::string_view json, const Options* options = &Options::gDefaults) {
    Parser parser{options};
    parser.parse(json);
    return parser.steal();
  }

  static std::shared_ptr<Builder> fromJson(
    const char* start, size_t size,
    const Options* options = &Options::gDefaults) {
    return fromJson({start, size}, options);
  }

  static std::shared_ptr<Builder> fromJson(
    const uint8_t* start, size_t size,
    const Options* options = &Options::gDefaults) {
    Parser parser(options);
    parser.parse(start, size);
    return parser.steal();
  }

  ValueLength parse(std::string_view json, bool multi = false) {
    return parse(reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                 multi);
  }

  ValueLength parse(const char* start, size_t size, bool multi = false) {
    return parse(reinterpret_cast<const uint8_t*>(start), size, multi);
  }

  ValueLength parse(const uint8_t* start, size_t size, bool multi = false) {
    _start = start;
    _size = size;
    _pos = 0;
    _nesting = 0;
    if (options->clear_builder_before_parse) {
      _builder->clear();
    }
    return parseInternal(multi);
  }

  // We probably want a parse from stream at some stage...
  // Not with this high-performance two-pass approach. :-(
  std::shared_ptr<Builder> steal() {
    // Parser object is broken after a steal()
    auto builder = std::move(_builder);
    SDB_ASSERT(!_builder);
    return builder;
  }

  // Beware, only valid as long as you do not parse more, use steal
  // to move the data out!
  const uint8_t* start() { return _builder->start(); }

  // Returns the position at the time when the just reported error
  // occurred, only use when handling an exception.
  size_t errorPos() const { return _pos > 0 ? _pos - 1 : _pos; }

  void clear() { _builder->clear(); }

 private:
  int peek() const {
    if (_pos >= _size) {
      return -1;
    }
    return static_cast<int>(_start[_pos]);
  }

  int consume() {
    if (_pos >= _size) {
      return -1;
    }
    return static_cast<int>(_start[_pos++]);
  }

  void unconsume() { --_pos; }

  void reset() { _pos = 0; }

  ValueLength parseInternal(bool multi);

  bool isWhiteSpace(uint8_t i) const noexcept {
    return (i == ' ' || i == '\t' || i == '\n' || i == '\r');
  }

  // skips over all following whitespace tokens but does not consume the
  // byte following the whitespace
  int skipWhiteSpace(const char*);

  void parseTrue() {
    // Called, when main mode has just seen a 't', need to see "rue" next
    if (consume() != 'r' || consume() != 'u' || consume() != 'e') {
      throw Exception(Exception::kParseError, "Expecting 'true'");
    }
    _builder->addBool(true);
  }

  void parseFalse() {
    // Called, when main mode has just seen a 'f', need to see "alse" next
    if (consume() != 'a' || consume() != 'l' || consume() != 's' ||
        consume() != 'e') {
      throw Exception(Exception::kParseError, "Expecting 'false'");
    }
    _builder->addBool(false);
  }

  void parseNull() {
    // Called, when main mode has just seen a 'n', need to see "ull" next
    if (consume() != 'u' || consume() != 'l' || consume() != 'l') {
      throw Exception(Exception::kParseError, "Expecting 'null'");
    }
    _builder->addNull();
  }

  void scanDigits(ParsedNumber& value) {
    while (true) {
      int i = consume();
      if (i < 0) {
        return;
      }
      if (i < '0' || i > '9') {
        unconsume();
        return;
      }
      value.addDigit(i);
    }
  }

  double scanDigitsFractional() {
    double pot = 0.1;
    double x = 0.0;
    while (true) {
      int i = consume();
      if (i < 0) {
        return x;
      }
      if (i < '0' || i > '9') {
        unconsume();
        return x;
      }
      x = x + pot * (i - '0');
      pot /= 10.0;
    }
  }

  inline int getOneOrThrow(const char* msg) {
    int i = consume();
    if (i < 0) {
      throw Exception(Exception::kParseError, msg);
    }
    return i;
  }

  void increaseNesting();

  void decreaseNesting() noexcept;

  void parseNumber();

  void parseString();

  void parseArray();

  void parseObject();

  void parseJson();
};

}  // namespace vpack
