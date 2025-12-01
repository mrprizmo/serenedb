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

#include <cstdlib>

#include "basics/common.h"
#include "error_code.h"

namespace sdb {

enum CsvParserState {
  kCsvParserBol,
  kCsvParserBoL2,
  kCsvParserBof,
  kCsvParserWithinField,
  kCsvParserWithinQuotedField,
  kCsvParserCorrupted
};

struct CsvParser {
  CsvParserState state;

  char quote;
  char separator;
  bool use_quote;
  bool use_backslash;

  char* begin;    // beginning of the input buffer
  char* start;    // start of the unproccessed part
  char* written;  // pointer to currently written character
  char* current;  // pointer to currently processed character
  char* stop;     // end of unproccessed part
  char* end;      // end of the input buffer

  size_t row;
  size_t column;

  void* data_begin;
  void* data_add;
  void* data_end;

  void (*begin_cb)(CsvParser*, size_t row);
  void (*add_cb)(CsvParser*, const char*, size_t, size_t row, size_t column,
                 bool escaped);
  void (*end_cb)(CsvParser*, const char*, size_t, size_t row, size_t column,
                 bool escaped);

  size_t n_resize;
  size_t n_memmove;
  size_t n_memcpy;
  void* data;
};

void InitCsvParser(CsvParser*, void (*)(CsvParser*, size_t),
                   void (*)(CsvParser*, const char*, size_t, size_t, size_t,
                            bool),
                   void (*)(CsvParser*, const char*, size_t, size_t, size_t,
                            bool),
                   void* v_data);

void DestroyCsvParser(CsvParser* parser) noexcept;

// note that the separator string must be valid until the parser is destroyed
void SetSeparatorCsvParser(CsvParser* parser, char separator);

// set the quote character
void SetQuoteCsvParser(CsvParser* parser, char quote, bool use_quote);

// whether or not a backslash is used to escape quotes
void UseBackslashCsvParser(CsvParser* parser, bool value);

// parses a CSV line
ErrorCode ParseCsvString(CsvParser* parser, const char* line, size_t length);

}  // namespace sdb
