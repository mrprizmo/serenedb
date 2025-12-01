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

#include "csv.h"

#include <cstring>
#include <new>

#include "basics/debugging.h"
#include "basics/errors.h"

namespace sdb {

void InitCsvParser(CsvParser* parser, void (*begin)(CsvParser*, size_t),
                   void (*add)(CsvParser*, const char*, size_t, size_t, size_t,
                               bool),
                   void (*end)(CsvParser*, const char*, size_t, size_t, size_t,
                               bool),
                   void* v_data) {
  size_t length;

  parser->state = kCsvParserBol;
  parser->data = v_data;
  SetQuoteCsvParser(parser, '"', true);
  SetSeparatorCsvParser(parser, ';');
  UseBackslashCsvParser(parser, false);

  length = 1024;

  parser->row = 0;
  parser->column = 0;

  parser->begin = new (std::nothrow) char[length];

  parser->start = parser->begin;
  parser->written = parser->begin;
  parser->current = parser->begin;
  parser->stop = parser->begin;

  if (parser->begin == nullptr) {
    length = 0;
    parser->end = nullptr;
  } else {
    parser->end = parser->begin + length;
  }

  parser->data_begin = nullptr;
  parser->data_add = nullptr;
  parser->data_end = nullptr;

  parser->begin_cb = begin;
  parser->add_cb = add;
  parser->end_cb = end;

  parser->n_resize = 0;
  parser->n_memmove = 0;
  parser->n_memcpy = 0;
}

void DestroyCsvParser(CsvParser* parser) noexcept { delete[] parser->begin; }

void SetSeparatorCsvParser(CsvParser* parser, char separator) {
  parser->separator = separator;
}

void SetQuoteCsvParser(CsvParser* parser, char quote, bool use_quote) {
  parser->quote = quote;
  parser->use_quote = use_quote;
}

void UseBackslashCsvParser(CsvParser* parser, bool value) {
  parser->use_backslash = value;
}

ErrorCode ParseCsvString(CsvParser* parser, const char* line, size_t length) {
  char* ptr;
  char* qtr;

  // append line to buffer
  if (line != nullptr) {
    SDB_ASSERT(parser->begin <= parser->start);
    SDB_ASSERT(parser->start <= parser->written);
    SDB_ASSERT(parser->written <= parser->current);
    SDB_ASSERT(parser->current <= parser->stop);
    SDB_ASSERT(parser->stop <= parser->end);

    // there is enough room between STOP and END
    if (parser->stop + length <= parser->end) {
      memcpy(parser->stop, line, length);

      parser->stop += length;
      parser->n_memcpy++;
    } else {
      size_t l1 = parser->start - parser->begin;
      size_t l2 = parser->end - parser->stop;
      size_t l3;

      // not enough room, but enough room between BEGIN and START plus STOP
      // and END
      if (length <= l1 + l2) {
        l3 = parser->stop - parser->start;

        if (0 < l3) {
          memmove(parser->begin, parser->start, l3);
        }

        memcpy(parser->begin + l3, line, length);

        parser->start = parser->begin;
        parser->written = parser->written - l1;
        parser->current = parser->current - l1;
        parser->stop = parser->begin + l3 + length;
        parser->n_memmove++;
      }

      // really not enough room
      else {
        size_t l4, l5;

        l2 = parser->stop - parser->start;
        l3 = parser->end - parser->begin + length;
        l4 = parser->written - parser->start;
        l5 = parser->current - parser->start;

        ptr = new (std::nothrow) char[l3];

        if (ptr == nullptr) {
          return ERROR_OUT_OF_MEMORY;
        }

        memcpy(ptr, parser->start, l2);
        memcpy(ptr + l2, line, length);
        delete[] parser->begin;

        parser->begin = ptr;
        parser->start = ptr;
        parser->written = ptr + l4;
        parser->current = ptr + l5;
        parser->stop = ptr + l2 + length;
        parser->end = ptr + l3;
        parser->n_resize++;
      }
    }

    // start parsing or continue
    ptr = parser->current;
    qtr = parser->written;

    while (true) {
      switch (parser->state) {
        case kCsvParserBol:
          if (ptr == parser->stop) {
            parser->written = ptr;
            parser->current = ptr;
            return ERROR_OK;
          }

          parser->begin_cb(parser, parser->row);

          parser->column = 0;
          parser->state = kCsvParserBof;

          break;

        case kCsvParserBoL2:
          if (ptr == parser->stop) {
            parser->written = ptr;
            parser->current = ptr;
            return ERROR_OK;
          }

          if (*ptr == '\n') {
            ptr++;
          }
          parser->state = kCsvParserBol;

          break;

        case kCsvParserBof:
          if (ptr == parser->stop) {
            parser->written = ptr;
            parser->current = ptr;
            return ERROR_CORRUPTED_CSV;
          }

          else if (parser->use_quote && *ptr == parser->quote) {
            if (ptr + 1 == parser->stop) {
              parser->written = qtr;
              parser->current = ptr;
              return ERROR_CORRUPTED_CSV;
            }

            parser->state = kCsvParserWithinQuotedField;
            parser->start = ++ptr;

            qtr = parser->written = ptr;
          } else {
            parser->state = kCsvParserWithinField;
            parser->start = ptr;

            qtr = parser->written = ptr;
          }

          break;

        case kCsvParserCorrupted:
          while (ptr < parser->stop && *ptr != parser->separator &&
                 *ptr != '\n') {
            ptr++;
          }

          // found separator or eol
          if (ptr < parser->stop) {
            // found separator
            if (*ptr == parser->separator) {
              ptr++;

              parser->state = kCsvParserBof;
            }

            // found eol
            else {
              ptr++;

              parser->row++;
              parser->state = kCsvParserBol;
            }
          }

          // need more input
          else {
            parser->written = qtr;
            parser->current = ptr;
            return ERROR_OK;
          }

          break;

        case kCsvParserWithinField:
          while (ptr < parser->stop && *ptr != parser->separator &&
                 *ptr != '\r' && *ptr != '\n') {
            *qtr++ = *ptr++;
          }

          // found separator or eol
          if (ptr < parser->stop) {
            // found separator
            if (*ptr == parser->separator) {
              *qtr = '\0';

              parser->add_cb(parser, parser->start, qtr - parser->start,
                             parser->row, parser->column, false);

              ptr++;
              parser->column++;
              parser->state = kCsvParserBof;
            }

            // found eol
            else {
              char c = *ptr;
              *qtr = '\0';

              parser->end_cb(parser, parser->start, qtr - parser->start,
                             parser->row, parser->column, false);
              parser->row++;
              if (c == '\r') {
                parser->state = kCsvParserBoL2;
              } else {
                parser->state = kCsvParserBol;
              }

              ptr++;
            }
          }

          // need more input
          else {
            parser->written = qtr;
            parser->current = ptr;
            return ERROR_OK;
          }

          break;

        case kCsvParserWithinQuotedField:
          SDB_ASSERT(parser->use_quote);

          while (ptr < parser->stop && *ptr != parser->quote &&
                 (!parser->use_backslash || *ptr != '\\')) {
            *qtr++ = *ptr++;
          }

          // found quote or a backslash, need at least another quote, a
          // separator, or an eol
          if (ptr + 1 < parser->stop) {
            bool found_backslash = (parser->use_backslash && *ptr == '\\');

            ++ptr;

            if (found_backslash) {
              if (*ptr == parser->quote || *ptr == '\\') {
                // backslash-escaped quote or literal backslash
                *qtr++ = *ptr;
                ptr++;
                break;
              }
            } else if (*ptr == parser->quote) {
              // a real quote
              *qtr++ = parser->quote;
              ptr++;
              break;
            }

            // ignore spaces
            while ((*ptr == ' ' || *ptr == '\t') && (ptr + 1) < parser->stop) {
              ++ptr;
            }

            // found separator
            if (*ptr == parser->separator) {
              *qtr = '\0';

              parser->add_cb(parser, parser->start, qtr - parser->start,
                             parser->row, parser->column, true);

              ptr++;
              parser->column++;
              parser->state = kCsvParserBof;
            }

            else if (*ptr == '\r' || *ptr == '\n') {
              char c = *ptr;
              *qtr = '\0';

              parser->end_cb(parser, parser->start, qtr - parser->start,
                             parser->row, parser->column, true);
              parser->row++;

              if (c == '\r') {
                parser->state = kCsvParserBoL2;
              } else {
                parser->state = kCsvParserBol;
              }

              ptr++;
            }

            // ups
            else {
              parser->state = kCsvParserCorrupted;
            }
          }

          // need more input
          else {
            parser->written = qtr;
            parser->current = ptr;
            return ERROR_OK;
          }

          break;
      }
    }
  }

  return ERROR_CORRUPTED_CSV;
}

}  // namespace sdb
