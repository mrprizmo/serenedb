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

#include "strings.h"

#include <cstring>
#include <ctime>

#include "basics/operating-system.h"
#include "basics/system-functions.h"
#include "basics/utf8_helper.h"

namespace sdb {
namespace {

int IntHex(char ch, int error_value) {
  if ('0' <= ch && ch <= '9') {
    return ch - '0';
  } else if ('A' <= ch && ch <= 'F') {
    return ch - 'A' + 10;
  } else if ('a' <= ch && ch <= 'f') {
    return ch - 'a' + 10;
  }

  return error_value;
}

void DecodeUnicodeEscape(char** dst, const char* src) {
  int i1;
  int i2;
  int i3;
  int i4;
  uint16_t n;

  i1 = IntHex(src[0], 0);
  i2 = IntHex(src[1], 0);
  i3 = IntHex(src[2], 0);
  i4 = IntHex(src[3], 0);

  n = ((i1 & 0xF) << 12) | ((i2 & 0xF) << 8) | ((i3 & 0xF) << 4) | (i4 & 0xF);

  if (n <= 0x7F) {
    *(*dst) = n & 0x7F;
  } else if (n <= 0x7FF) {
    *(*dst)++ = 0xC0 + (n >> 6);
    *(*dst) = 0x80 + (n & 0x3F);

  } else {
    *(*dst)++ = 0xE0 + (n >> 12);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst) = 0x80 + (n & 0x3F);
  }
}

void DecodeSurrogatePair(char** dst, const char* src1, const char* src2) {
  int i1;
  int i2;
  int i3;
  int i4;
  uint32_t n1;
  uint32_t n2;
  uint32_t n;

  i1 = IntHex(src1[0], 0);
  i2 = IntHex(src1[1], 0);
  i3 = IntHex(src1[2], 0);
  i4 = IntHex(src1[3], 0);

  n1 = ((i1 & 0xF) << 12) | ((i2 & 0xF) << 8) | ((i3 & 0xF) << 4) | (i4 & 0xF);
  n1 -= 0xD800;

  i1 = IntHex(src2[0], 0);
  i2 = IntHex(src2[1], 0);
  i3 = IntHex(src2[2], 0);
  i4 = IntHex(src2[3], 0);

  n2 = ((i1 & 0xF) << 12) | ((i2 & 0xF) << 8) | ((i3 & 0xF) << 4) | (i4 & 0xF);
  n2 -= 0xDC00;

  n = 0x10000 + ((n1 << 10) | n2);

  if (n <= 0x7F) {
    *(*dst) = n & 0x7F;
  } else if (n <= 0x7FF) {
    *(*dst)++ = 0xC0 + (n >> 6);
    *(*dst) = 0x80 + (n & 0x3F);
  } else if (n <= 0xFFFF) {
    *(*dst)++ = 0xE0 + (n >> 12);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst) = 0x80 + (n & 0x3F);
  } else {
    *(*dst)++ = 0xF0 + (n >> 18);
    *(*dst)++ = 0x80 + ((n >> 12) & 0x3F);
    *(*dst)++ = 0x80 + ((n >> 6) & 0x3F);
    *(*dst) = 0x80 + (n & 0x3F);
  }
}

}  // namespace

char* DuplicateString(const char* value, size_t length) {
  char* result = new (std::nothrow) char[length + 1];

  if (result != nullptr) {
    memcpy(result, value, length);
    result[length] = '\0';
  }

  return result;
}

void CopyString(char* dst, const char* src, size_t length) {
  *dst = '\0';
  strncat(dst, src, length);
}

void FreeString(char* value) noexcept { delete[] value; }

size_t UnescapeUtf8StringInPlace(char* buffer, const char* in,
                                 size_t in_length) {
  char* qtr = buffer;
  const char* ptr;
  const char* end;

  for (ptr = in, end = ptr + in_length; ptr < end; ++ptr, ++qtr) {
    if (*ptr == '\\' && ptr + 1 < end) {
      ++ptr;

      switch (*ptr) {
        case 'b':
          *qtr = '\b';
          break;

        case 'f':
          *qtr = '\f';
          break;

        case 'n':
          *qtr = '\n';
          break;

        case 'r':
          *qtr = '\r';
          break;

        case 't':
          *qtr = '\t';
          break;

        case 'u':
          // expecting at least 6 characters: \uXXXX
          if (ptr + 4 < end) {
            // check, if we have a surrogate pair
            if (ptr + 10 < end) {
              bool sp;
              char c1 = ptr[1];

              sp = (c1 == 'd' || c1 == 'D');

              if (sp) {
                char c2 = ptr[2];
                sp &= (c2 == '8' || c2 == '9' || c2 == 'A' || c2 == 'a' ||
                       c2 == 'B' || c2 == 'b');
              }

              if (sp) {
                char c3 = ptr[7];

                sp &= (ptr[5] == '\\' && ptr[6] == 'u');
                sp &= (c3 == 'd' || c3 == 'D');
              }

              if (sp) {
                char c4 = ptr[8];
                sp &= (c4 == 'C' || c4 == 'c' || c4 == 'D' || c4 == 'd' ||
                       c4 == 'E' || c4 == 'e' || c4 == 'F' || c4 == 'f');
              }

              if (sp) {
                DecodeSurrogatePair(&qtr, ptr + 1, ptr + 7);
                ptr += 10;
              } else {
                DecodeUnicodeEscape(&qtr, ptr + 1);
                ptr += 4;
              }
            } else {
              DecodeUnicodeEscape(&qtr, ptr + 1);
              ptr += 4;
            }
          }
          // ignore wrong format
          else {
            *qtr = *ptr;
          }
          break;

        default:
          // this includes cases \/, \\, and \"
          *qtr = *ptr;
          break;
      }

      continue;
    }

    *qtr = *ptr;
  }

  return static_cast<size_t>(qtr - buffer);
}

size_t CharLengthUtf8String(const char* in, size_t length) {
  const unsigned char* p = reinterpret_cast<const unsigned char*>(in);
  const unsigned char* e = p + length;
  size_t chars = 0;

  while (p < e) {
    unsigned char c = *p;

    if (c < 128) {
      // single byte
      p++;
    } else if (c < 224) {
      p += 2;
    } else if (c < 240) {
      p += 3;
    } else if (c < 248) {
      p += 4;
    } else {
      // invalid UTF-8 sequence
      break;
    }

    ++chars;
  }

  return chars;
}

char* UnescapeUtf8String(const char* in, size_t in_length, size_t* out_length,
                         bool normalize) {
  char* buffer = new (std::nothrow) char[in_length + 1];

  if (buffer == nullptr) {
    return nullptr;
  }

  *out_length = UnescapeUtf8StringInPlace(buffer, in, in_length);
  buffer[*out_length] = '\0';

  if (normalize && *out_length > 0) {
    size_t tmp_length = 0;
    char* utf8_nfc = NormalizeUtf8ToNFC(buffer, *out_length, &tmp_length);

    if (utf8_nfc != nullptr) {
      *out_length = tmp_length;
      delete[] buffer;
      buffer = utf8_nfc;
    }
    // intentionally falls through
  }

  return buffer;
}

std::string StringTimeStamp(double stamp, bool use_local_time) {
  char buffer[32];
  size_t len;
  struct tm tb;
  time_t tt = static_cast<time_t>(stamp);

  if (use_local_time) {
    utilities::GetLocalTime(tt, &tb);
  } else {
    utilities::GetGmtime(tt, &tb);
  }
  len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);

  return std::string(buffer, len);
}

static const char* const kHex = "0123456789ABCDEF";

size_t StringUInt32HexInPlace(uint32_t attr, char* buffer) {
  char* p = buffer;

  if (0x10000000U <= attr) {
    *p++ = kHex[(attr / 0x10000000U) % 0x10];
  }
  if (0x1000000U <= attr) {
    *p++ = kHex[(attr / 0x1000000U) % 0x10];
  }
  if (0x100000U <= attr) {
    *p++ = kHex[(attr / 0x100000U) % 0x10];
  }
  if (0x10000U <= attr) {
    *p++ = kHex[(attr / 0x10000U) % 0x10];
  }
  if (0x1000U <= attr) {
    *p++ = kHex[(attr / 0x1000U) % 0x10];
  }
  if (0x100U <= attr) {
    *p++ = kHex[(attr / 0x100U) % 0x10];
  }
  if (0x10U <= attr) {
    *p++ = kHex[(attr / 0x10U) % 0x10];
  }

  *p++ = kHex[attr % 0x10];
  *p = '\0';

  return static_cast<size_t>(p - buffer);
}

size_t StringUInt64HexInPlace(uint64_t attr, char* buffer) {
  char* p = buffer;

  if (0x1000000000000000ULL <= attr) {
    *p++ = kHex[(attr / 0x1000000000000000ULL) % 0x10];
  }
  if (0x100000000000000ULL <= attr) {
    *p++ = kHex[(attr / 0x100000000000000ULL) % 0x10];
  }
  if (0x10000000000000ULL <= attr) {
    *p++ = kHex[(attr / 0x10000000000000ULL) % 0x10];
  }
  if (0x1000000000000ULL <= attr) {
    *p++ = kHex[(attr / 0x1000000000000ULL) % 0x10];
  }
  if (0x100000000000ULL <= attr) {
    *p++ = kHex[(attr / 0x100000000000ULL) % 0x10];
  }
  if (0x10000000000ULL <= attr) {
    *p++ = kHex[(attr / 0x10000000000ULL) % 0x10];
  }
  if (0x1000000000ULL <= attr) {
    *p++ = kHex[(attr / 0x1000000000ULL) % 0x10];
  }
  if (0x100000000ULL <= attr) {
    *p++ = kHex[(attr / 0x100000000ULL) % 0x10];
  }
  if (0x10000000ULL <= attr) {
    *p++ = kHex[(attr / 0x10000000ULL) % 0x10];
  }
  if (0x1000000ULL <= attr) {
    *p++ = kHex[(attr / 0x1000000ULL) % 0x10];
  }
  if (0x100000ULL <= attr) {
    *p++ = kHex[(attr / 0x100000ULL) % 0x10];
  }
  if (0x10000ULL <= attr) {
    *p++ = kHex[(attr / 0x10000ULL) % 0x10];
  }
  if (0x1000ULL <= attr) {
    *p++ = kHex[(attr / 0x1000ULL) % 0x10];
  }
  if (0x100ULL <= attr) {
    *p++ = kHex[(attr / 0x100ULL) % 0x10];
  }
  if (0x10ULL <= attr) {
    *p++ = kHex[(attr / 0x10ULL) % 0x10];
  }

  *p++ = kHex[attr % 0x10];
  *p = '\0';

  return (p - buffer);
}

}  // namespace sdb
