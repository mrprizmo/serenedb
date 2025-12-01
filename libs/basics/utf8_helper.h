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

#include <absl/strings/internal/utf8.h>
#include <stddef.h>
#include <unicode/coll.h>
#include <unicode/regex.h>
#include <unicode/umachine.h>

#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>

#include "basics/common.h"

namespace sdb {
namespace basics {

enum class LanguageType {
  Invalid,
  Default,  // TODO remove it
  ICU,
};

class Utf8Helper {
  Utf8Helper(const Utf8Helper&) = delete;
  Utf8Helper& operator=(const Utf8Helper&) = delete;

  Utf8Helper() noexcept;

 public:
  static Utf8Helper gDefault;

  ~Utf8Helper();

  int compare(const char* l, size_t l_len, const char* r, size_t r_len) const;

  /// lang -- lowercase two-letter or three-letter ISO-639 code.
  /// This parameter can instead be an ICU style C locale (e.g. "en_US")
  /// icu -- data file to be loaded by the application
  /// type -- type of language. Now supports DEFAULT and ICU only collation
  bool setCollatorLanguage(const std::string& lang, LanguageType type,
                           void* icu);

  std::string getCollatorLanguage();
  std::string getCollatorCountry();

  std::unique_ptr<icu::RegexMatcher> buildMatcher(std::string_view pattern);

  bool matches(icu::RegexMatcher*, const char* pattern, size_t pattern_len,
               bool partial, bool& error);

  std::string replace(icu::RegexMatcher*, const char* pattern,
                      size_t pattern_len, const char* replacement,
                      size_t replacement_len, bool partial, bool& error);

  static void appendUtf8Character(std::string& result, uint32_t ch) {
    char rune[4];
    const auto len = absl::strings_internal::EncodeUTF8Char(rune, ch);
    result.append(rune, len);
  }

#ifndef SDB_GTEST
 private:
#endif
  icu::Collator* _coll = nullptr;  // NOLINT
};

}  // namespace basics

UChar* Utf8ToUChar(const char* utf8, size_t in_len, size_t* out_len,
                   UErrorCode* status = nullptr);
UChar* Utf8ToUChar(const char* utf8, size_t in_len, UChar* buffer,
                   size_t buffer_size, size_t* out_len,
                   UErrorCode* status = nullptr);
char* UCharToUtf8(const UChar* uchar, size_t in_len, size_t* out_len,
                  UErrorCode* status = nullptr);

char* NormalizeUtf16ToNfc(const uint16_t* utf16, size_t in_len, size_t* out_len,
                          UErrorCode* status = nullptr);

char* NormalizeUtf8ToNFC(const char* utf8, size_t in_len, size_t* out_len,
                         UErrorCode* status = nullptr);

std::string NormalizeUtf8ToNFC(std::string_view value);

inline int CompareUtf8(const char* l, size_t l_len, const char* r,
                       size_t r_len) {
  return basics::Utf8Helper::gDefault.compare(l, l_len, r, r_len);
}

}  // namespace sdb
