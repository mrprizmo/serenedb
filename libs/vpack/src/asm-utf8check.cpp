////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright Daniel Lemire
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
/// Copyright holder is Daniel Lemire
///
////////////////////////////////////////////////////////////////////////////////

#include "asm-utf8check.h"

#if VPACK_ASM_OPTIMIZATIONS == 1
#include <cstring>
#ifdef __aarch64__
#include <sse2neon.h>
#else
#include <x86intrin.h>
#endif

/*
 * legal utf-8 byte sequence
 * http://www.unicode.org/versions/Unicode6.0.0/ch03.pdf - page 94
 *
 *  Code Points        1st       2s       3s       4s
 * U+0000..U+007F     00..7F
 * U+0080..U+07FF     C2..DF   80..BF
 * U+0800..U+0FFF     E0       A0..BF   80..BF
 * U+1000..U+CFFF     E1..EC   80..BF   80..BF
 * U+D000..U+D7FF     ED       80..9F   80..BF
 * U+E000..U+FFFF     EE..EF   80..BF   80..BF
 * U+10000..U+3FFFF   F0       90..BF   80..BF   80..BF
 * U+40000..U+FFFFF   F1..F3   80..BF   80..BF   80..BF
 * U+100000..U+10FFFF F4       80..8F   80..BF   80..BF
 *
 */

namespace vpack {

// all byte values must be no larger than 0xF4
static inline void CheckSmallerThan0xF4(__m128i current_bytes,
                                        __m128i* has_error) {
  // unsigned, saturates to 0 below max
  *has_error =
    _mm_or_si128(*has_error, _mm_subs_epu8(current_bytes, _mm_set1_epi8(0xF4)));
}

static inline __m128i ContinuationLengths(__m128i high_nibbles) {
  return _mm_shuffle_epi8(
    _mm_setr_epi8(1, 1, 1, 1, 1, 1, 1, 1,  // 0xxx (ASCII)
                  0, 0, 0, 0,              // 10xx (continuation)
                  2, 2,                    // 110x
                  3,                       // 1110
                  4),  // 1111, next should be 0 (not checked here)
    high_nibbles);
}

static inline __m128i CarryContinuations(__m128i initial_lengths,
                                         __m128i previous_carries) {
  __m128i right1 =
    _mm_subs_epu8(_mm_alignr_epi8(initial_lengths, previous_carries, 16 - 1),
                  _mm_set1_epi8(1));
  __m128i sum = _mm_add_epi8(initial_lengths, right1);

  __m128i right2 = _mm_subs_epu8(_mm_alignr_epi8(sum, previous_carries, 16 - 2),
                                 _mm_set1_epi8(2));
  return _mm_add_epi8(sum, right2);
}

static inline void CheckContinuations(__m128i initial_lengths, __m128i carries,
                                      __m128i* has_error) {
  // overlap || underlap
  // carry > length && length > 0 || !(carry > length) && !(length > 0)
  // (carries > length) == (lengths > 0)
  __m128i overunder =
    _mm_cmpeq_epi8(_mm_cmpgt_epi8(carries, initial_lengths),
                   _mm_cmpgt_epi8(initial_lengths, _mm_setzero_si128()));

  *has_error = _mm_or_si128(*has_error, overunder);
}

// when 0xED is found, next byte must be no larger than 0x9F
// when 0xF4 is found, next byte must be no larger than 0x8F
// next byte must be continuation, ie sign bit is set, so signed < is ok
static inline void CheckFirstContinuationMax(__m128i current_bytes,
                                             __m128i off1_current_bytes,
                                             __m128i* has_error) {
  __m128i mask_ed = _mm_cmpeq_epi8(off1_current_bytes, _mm_set1_epi8(0xED));
  __m128i mask_f4 = _mm_cmpeq_epi8(off1_current_bytes, _mm_set1_epi8(0xF4));

  __m128i badfollow_ed =
    _mm_and_si128(_mm_cmpgt_epi8(current_bytes, _mm_set1_epi8(0x9F)), mask_ed);
  __m128i badfollow_f4 =
    _mm_and_si128(_mm_cmpgt_epi8(current_bytes, _mm_set1_epi8(0x8F)), mask_f4);

  *has_error =
    _mm_or_si128(*has_error, _mm_or_si128(badfollow_ed, badfollow_f4));
}

// map off1_hibits => error condition
// hibits     off1    cur
// C       => < C2 && true
// E       => < E1 && < A0
// F       => < F1 && < 90
// else      false && false
static inline void CheckOverlong(__m128i current_bytes,
                                 __m128i off1_current_bytes, __m128i hibits,
                                 __m128i previous_hibits, __m128i* has_error) {
  __m128i off1_hibits = _mm_alignr_epi8(hibits, previous_hibits, 16 - 1);
  __m128i initial_mins = _mm_shuffle_epi8(
    _mm_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                  -128, -128,  // 10xx => false
                  0xC2, -128,  // 110x
                  0xE1,        // 1110
                  0xF1),
    off1_hibits);

  __m128i initial_under = _mm_cmpgt_epi8(initial_mins, off1_current_bytes);

  __m128i second_mins = _mm_shuffle_epi8(
    _mm_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                  -128, -128,  // 10xx => false
                  127, 127,    // 110x => true
                  0xA0,        // 1110
                  0x90),
    off1_hibits);
  __m128i second_under = _mm_cmpgt_epi8(second_mins, current_bytes);
  *has_error =
    _mm_or_si128(*has_error, _mm_and_si128(initial_under, second_under));
}

struct ProcessedUtfBytes {
  __m128i rawbytes;
  __m128i high_nibbles;
  __m128i carried_continuations;
};

static inline void CountNibbles(__m128i bytes,
                                struct ProcessedUtfBytes* answer) {
  answer->rawbytes = bytes;
  answer->high_nibbles =
    _mm_and_si128(_mm_srli_epi16(bytes, 4), _mm_set1_epi8(0x0F));
}

// check whether the current bytes are valid UTF-8
// at the end of the function, previous gets updated
static struct ProcessedUtfBytes CheckUtF8Bytes(
  __m128i current_bytes, struct ProcessedUtfBytes* previous,
  __m128i* has_error) {
  struct ProcessedUtfBytes pb;
  CountNibbles(current_bytes, &pb);

  CheckSmallerThan0xF4(current_bytes, has_error);

  __m128i initial_lengths = ContinuationLengths(pb.high_nibbles);

  pb.carried_continuations =
    CarryContinuations(initial_lengths, previous->carried_continuations);

  CheckContinuations(initial_lengths, pb.carried_continuations, has_error);

  __m128i off1_current_bytes =
    _mm_alignr_epi8(pb.rawbytes, previous->rawbytes, 16 - 1);
  CheckFirstContinuationMax(current_bytes, off1_current_bytes, has_error);

  CheckOverlong(current_bytes, off1_current_bytes, pb.high_nibbles,
                previous->high_nibbles, has_error);
  return pb;
}

bool ValidateUtf8FastSse42(const uint8_t* src, size_t len) {
  size_t i = 0;
  __m128i has_error = _mm_setzero_si128();
  struct ProcessedUtfBytes previous = {
    .rawbytes = _mm_setzero_si128(),
    .high_nibbles = _mm_setzero_si128(),
    .carried_continuations = _mm_setzero_si128()};
  if (len >= 16) {
    for (; i <= len - 16; i += 16) {
      __m128i current_bytes = _mm_loadu_si128((const __m128i*)(src + i));
      previous = CheckUtF8Bytes(current_bytes, &previous, &has_error);
    }
  }

  // last part
  if (i < len) {
    char buffer[16];
    memset(buffer, 0, 16);
    memcpy(buffer, src + i, len - i);
    __m128i current_bytes = _mm_loadu_si128((const __m128i*)(buffer));
    previous = CheckUtF8Bytes(current_bytes, &previous, &has_error);
  } else {
    has_error = _mm_or_si128(
      _mm_cmpgt_epi8(
        previous.carried_continuations,
        _mm_setr_epi8(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 1)),
      has_error);
  }

  return _mm_testz_si128(has_error, has_error);
}

#ifdef __AVX2__

/*****************************/
static inline __m256i PushLastByteOfAToB(__m256i a, __m256i b) {
  return _mm256_alignr_epi8(b, _mm256_permute2x128_si256(a, b, 0x21), 15);
}

static inline __m256i PushLast2bytesOfAToB(__m256i a, __m256i b) {
  return _mm256_alignr_epi8(b, _mm256_permute2x128_si256(a, b, 0x21), 14);
}

// all byte values must be no larger than 0xF4
static inline void AvxcheckSmallerThan0xF4(__m256i current_bytes,
                                           __m256i* has_error) {
  // unsigned, saturates to 0 below max
  *has_error = _mm256_or_si256(
    *has_error, _mm256_subs_epu8(current_bytes, _mm256_set1_epi8(0xF4)));
}

static inline __m256i AvxcontinuationLengths(__m256i high_nibbles) {
  return _mm256_shuffle_epi8(
    _mm256_setr_epi8(1, 1, 1, 1, 1, 1, 1, 1,  // 0xxx (ASCII)
                     0, 0, 0, 0,              // 10xx (continuation)
                     2, 2,                    // 110x
                     3,                       // 1110
                     4,  // 1111, next should be 0 (not checked here)
                     1, 1, 1, 1, 1, 1, 1, 1,  // 0xxx (ASCII)
                     0, 0, 0, 0,              // 10xx (continuation)
                     2, 2,                    // 110x
                     3,                       // 1110
                     4  // 1111, next should be 0 (not checked here)
                     ),
    high_nibbles);
}

static inline __m256i AvxcarryContinuations(__m256i initial_lengths,
                                            __m256i previous_carries) {
  __m256i right1 = _mm256_subs_epu8(
    PushLastByteOfAToB(previous_carries, initial_lengths), _mm256_set1_epi8(1));
  __m256i sum = _mm256_add_epi8(initial_lengths, right1);

  __m256i right2 = _mm256_subs_epu8(PushLast2bytesOfAToB(previous_carries, sum),
                                    _mm256_set1_epi8(2));
  return _mm256_add_epi8(sum, right2);
}

static inline void AvxcheckContinuations(__m256i initial_lengths,
                                         __m256i carries, __m256i* has_error) {
  // overlap || underlap
  // carry > length && length > 0 || !(carry > length) && !(length > 0)
  // (carries > length) == (lengths > 0)
  __m256i overunder = _mm256_cmpeq_epi8(
    _mm256_cmpgt_epi8(carries, initial_lengths),
    _mm256_cmpgt_epi8(initial_lengths, _mm256_setzero_si256()));

  *has_error = _mm256_or_si256(*has_error, overunder);
}

// when 0xED is found, next byte must be no larger than 0x9F
// when 0xF4 is found, next byte must be no larger than 0x8F
// next byte must be continuation, ie sign bit is set, so signed < is ok
static inline void AvxcheckFirstContinuationMax(__m256i current_bytes,
                                                __m256i off1_current_bytes,
                                                __m256i* has_error) {
  __m256i mask_ed =
    _mm256_cmpeq_epi8(off1_current_bytes, _mm256_set1_epi8(0xED));
  __m256i mask_f4 =
    _mm256_cmpeq_epi8(off1_current_bytes, _mm256_set1_epi8(0xF4));

  __m256i badfollow_ed = _mm256_and_si256(
    _mm256_cmpgt_epi8(current_bytes, _mm256_set1_epi8(0x9F)), mask_ed);
  __m256i badfollow_f4 = _mm256_and_si256(
    _mm256_cmpgt_epi8(current_bytes, _mm256_set1_epi8(0x8F)), mask_f4);

  *has_error =
    _mm256_or_si256(*has_error, _mm256_or_si256(badfollow_ed, badfollow_f4));
}

// map off1_hibits => error condition
// hibits     off1    cur
// C       => < C2 && true
// E       => < E1 && < A0
// F       => < F1 && < 90
// else      false && false
static inline void AvxcheckOverlong(__m256i current_bytes,
                                    __m256i off1_current_bytes, __m256i hibits,
                                    __m256i previous_hibits,
                                    __m256i* has_error) {
  __m256i off1_hibits = PushLastByteOfAToB(previous_hibits, hibits);
  __m256i initial_mins = _mm256_shuffle_epi8(
    _mm256_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                     -128, -128,  // 10xx => false
                     0xC2, -128,  // 110x
                     0xE1,        // 1110
                     0xF1, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                     -128, -128, -128,  // 10xx => false
                     0xC2, -128,        // 110x
                     0xE1,              // 1110
                     0xF1),
    off1_hibits);

  __m256i initial_under = _mm256_cmpgt_epi8(initial_mins, off1_current_bytes);

  __m256i second_mins = _mm256_shuffle_epi8(
    _mm256_setr_epi8(-128, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                     -128, -128,  // 10xx => false
                     127, 127,    // 110x => true
                     0xA0,        // 1110
                     0x90, -128, -128, -128, -128, -128, -128, -128, -128, -128,
                     -128, -128, -128,  // 10xx => false
                     127, 127,          // 110x => true
                     0xA0,              // 1110
                     0x90),
    off1_hibits);
  __m256i second_under = _mm256_cmpgt_epi8(second_mins, current_bytes);
  *has_error =
    _mm256_or_si256(*has_error, _mm256_and_si256(initial_under, second_under));
}

struct AvxProcessedUtfBytes {
  __m256i rawbytes;
  __m256i high_nibbles;
  __m256i carried_continuations;
};

static inline void AvxCountNibbles(__m256i bytes,
                                   struct AvxProcessedUtfBytes* answer) {
  answer->rawbytes = bytes;
  answer->high_nibbles =
    _mm256_and_si256(_mm256_srli_epi16(bytes, 4), _mm256_set1_epi8(0x0F));
}

// check whether the current bytes are valid UTF-8
// at the end of the function, previous gets updated
static struct AvxProcessedUtfBytes AvxcheckUtF8Bytes(
  __m256i current_bytes, struct AvxProcessedUtfBytes* previous,
  __m256i* has_error) {
  struct AvxProcessedUtfBytes pb;
  AvxCountNibbles(current_bytes, &pb);

  AvxcheckSmallerThan0xF4(current_bytes, has_error);

  __m256i initial_lengths = AvxcontinuationLengths(pb.high_nibbles);

  pb.carried_continuations =
    AvxcarryContinuations(initial_lengths, previous->carried_continuations);

  AvxcheckContinuations(initial_lengths, pb.carried_continuations, has_error);

  __m256i off1_current_bytes =
    PushLastByteOfAToB(previous->rawbytes, pb.rawbytes);
  AvxcheckFirstContinuationMax(current_bytes, off1_current_bytes, has_error);

  AvxcheckOverlong(current_bytes, off1_current_bytes, pb.high_nibbles,
                   previous->high_nibbles, has_error);
  return pb;
}

// check whether the current bytes are valid UTF-8
// at the end of the function, previous gets updated
static struct AvxProcessedUtfBytes AvxcheckUtF8BytesAsciipath(
  __m256i current_bytes, struct AvxProcessedUtfBytes* previous,
  __m256i* has_error) {
  if (_mm256_testz_si256(current_bytes,
                         _mm256_set1_epi8(0x80))) {  // fast ascii path
    *has_error = _mm256_or_si256(
      _mm256_cmpgt_epi8(
        previous->carried_continuations,
        _mm256_setr_epi8(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                         9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 1)),
      *has_error);
    return *previous;
  }

  struct AvxProcessedUtfBytes pb;
  AvxCountNibbles(current_bytes, &pb);

  AvxcheckSmallerThan0xF4(current_bytes, has_error);

  __m256i initial_lengths = AvxcontinuationLengths(pb.high_nibbles);

  pb.carried_continuations =
    AvxcarryContinuations(initial_lengths, previous->carried_continuations);

  AvxcheckContinuations(initial_lengths, pb.carried_continuations, has_error);

  __m256i off1_current_bytes =
    PushLastByteOfAToB(previous->rawbytes, pb.rawbytes);
  AvxcheckFirstContinuationMax(current_bytes, off1_current_bytes, has_error);

  AvxcheckOverlong(current_bytes, off1_current_bytes, pb.high_nibbles,
                   previous->high_nibbles, has_error);
  return pb;
}

bool ValidateUtf8FastAvxAsciipath(const char* src, size_t len) {
  size_t i = 0;
  __m256i has_error = _mm256_setzero_si256();
  struct AvxProcessedUtfBytes previous = {
    .rawbytes = _mm256_setzero_si256(),
    .high_nibbles = _mm256_setzero_si256(),
    .carried_continuations = _mm256_setzero_si256()};
  if (len >= 32) {
    for (; i <= len - 32; i += 32) {
      __m256i current_bytes = _mm256_loadu_si256((const __m256i*)(src + i));
      previous =
        AvxcheckUtF8BytesAsciipath(current_bytes, &previous, &has_error);
    }
  }

  // last part
  if (i < len) {
    char buffer[32];
    memset(buffer, 0, 32);
    memcpy(buffer, src + i, len - i);
    __m256i current_bytes = _mm256_loadu_si256((const __m256i*)(buffer));
    previous = AvxcheckUtF8Bytes(current_bytes, &previous, &has_error);
  } else {
    has_error = _mm256_or_si256(
      _mm256_cmpgt_epi8(
        previous.carried_continuations,
        _mm256_setr_epi8(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                         9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 1)),
      has_error);
  }

  return _mm256_testz_si256(has_error, has_error);
}

bool ValidateUtf8FastAvx(const uint8_t* src, size_t len) {
  size_t i = 0;
  __m256i has_error = _mm256_setzero_si256();
  struct AvxProcessedUtfBytes previous = {
    .rawbytes = _mm256_setzero_si256(),
    .high_nibbles = _mm256_setzero_si256(),
    .carried_continuations = _mm256_setzero_si256()};
  if (len >= 32) {
    for (; i <= len - 32; i += 32) {
      __m256i current_bytes = _mm256_loadu_si256((const __m256i*)(src + i));
      previous = AvxcheckUtF8Bytes(current_bytes, &previous, &has_error);
    }
  }

  // last part
  if (i < len) {
    char buffer[32];
    memset(buffer, 0, 32);
    memcpy(buffer, src + i, len - i);
    __m256i current_bytes = _mm256_loadu_si256((const __m256i*)(buffer));
    previous = AvxcheckUtF8Bytes(current_bytes, &previous, &has_error);
  } else {
    has_error = _mm256_or_si256(
      _mm256_cmpgt_epi8(
        previous.carried_continuations,
        _mm256_setr_epi8(9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9,
                         9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 1)),
      has_error);
  }

  return _mm256_testz_si256(has_error, has_error);
}

#endif  // __AVX2__

}  // namespace vpack

#endif  // VPACK_ASM_OPTIMIZATIONS == 1
