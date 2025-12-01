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

#include <absl/strings/cord.h>
#include <absl/strings/cord_buffer.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>

#include <string>
#include <string_view>

#include "basics/assert.h"
#include "basics/dtoa.h"
#include "basics/string_utils.h"

namespace sdb::basics {

class LenSink {
 public:
  void PushStr(std::string_view s) noexcept { _impl += s.size(); }

  void PushChr(char /*c*/) noexcept { ++_impl; }

  void PushI64(int64_t i) { PushInt(i); }

  void PushU64(uint64_t u) { PushInt(u); }

  void PushF64(double d) { _impl += dtoa_vpack(d, _buf) - _buf; }

  void PushSpaces(size_t n) noexcept { _impl += n; }

  size_t Impl() const noexcept { return _impl; }

 private:
  void PushInt(auto i) {
    _impl += absl::numbers_internal::FastIntToBuffer(i, _buf) - _buf;
  }

  size_t _impl = 0;
  char _buf[basics::kNumberStrMaxLen];
};

class StrSink {
 public:
  StrSink() = default;
  StrSink(std::string&& impl) : _impl{std::move(impl)} {}

  void PushStr(std::string_view s) { _impl.append(s); }

  void PushChr(char c) { _impl.push_back(c); }

  void PushI64(int64_t i) { PushInt(i); }

  void PushU64(uint64_t u) { PushInt(u); }

  void PushF64(double d) {
    const auto size = _impl.size();
    basics::StrAppend(_impl, basics::kNumberStrMaxLen);
    auto* const data = _impl.data();
    auto* const r = dtoa_vpack(d, data + size);
    _impl.erase(r - data);
  }

  void PushSpaces(size_t n) noexcept { _impl.append(n, ' '); }

  auto& Impl(this auto& self) noexcept { return self._impl; }

 protected:
  void PushInt(auto i) {
    const auto size = _impl.size();
    basics::StrAppend(_impl, basics::kIntStrMaxLen);
    auto* const data = _impl.data();
    auto* const r = absl::numbers_internal::FastIntToBuffer(i, data + size);
    _impl.erase(r - data);
  }

  std::string _impl;
};

struct CordSink {
 public:
  explicit CordSink() : _buf{createBuffer()}, _avail{_buf.available()} {}

  auto& Impl(this auto& self) noexcept { return self._impl; }

  void Finish() { FlushBuffer<true>(); }

  void PushStr(std::string_view s) {
    auto left = s.size();
    auto p = s.data();

    // TODO(gnusi): unroll loop to handle fast path?

    while (left) {
      // TODO(gnusi): sacrifice some bytes for performance?
      EnsureBuffer(1);
      const auto to_copy = std::min(_avail.Size(), left);
      std::memcpy(_avail.begin, p, to_copy);
      _avail.begin += to_copy;
      p += to_copy;
      left -= to_copy;
    }
  }

  void PushChr(char c) {
    if (_avail.Empty()) [[unlikely]] {
      FlushBuffer();
    }

    *_avail.begin++ = c;
  }

  void PushI64(int64_t i) { PushInt(i); }

  void PushU64(uint64_t u) { PushInt(u); }

  void PushF64(double d) {
    EnsureBuffer(basics::kNumberStrMaxLen);
    _avail.begin = dtoa_vpack(d, _avail.begin);
  }

  void PushSpaces(size_t n) noexcept {
    // TODO(gnusi): unroll loop to handle fast path?

    while (n) {
      // TODO(gnusi): sacrifice some bytes for performance?
      EnsureBuffer(1);
      const auto to_topy = std::min(_avail.Size(), n);
      std::memset(_avail.begin, ' ', to_topy);
      _avail.begin += to_topy;
      n -= to_topy;
    }
  }

 private:
  template<bool IsFinish = false>
  void FlushBuffer() {
    const auto size = _avail.begin - _buf.available().data();
    if (IsFinish && size == 0) {
      return;
    }
    SDB_ASSERT(size);
    _buf.SetLength(size);
    _impl.Append(std::move(_buf));
    if constexpr (!IsFinish) {
      _buf = createBuffer();
      _avail = _buf.available();
    }
  }

  void EnsureBuffer(size_t size) {
    SDB_ASSERT(size);
    SDB_ASSERT(size <= _buf.capacity());

    if (size <= _avail.Size()) [[likely]] {
      return;
    }

    FlushBuffer();
  }

  void PushInt(auto v) {
    EnsureBuffer(basics::kNumberStrMaxLen);
    _avail.begin = absl::numbers_internal::FastIntToBuffer(v, _avail.begin);
  }

  static absl::CordBuffer createBuffer() {
    return absl::CordBuffer::CreateWithDefaultLimit(
      absl::cord_internal::kMaxFlatLength);
  }

  struct Span {
    Span(auto span) noexcept : begin{span.data()}, end{span.end()} {}

    bool Empty() const noexcept { return begin == end; }
    size_t Size() const noexcept { return end - begin; }

    char* begin;
    char* end;
  };

  absl::Cord _impl;
  absl::CordBuffer _buf;
  Span _avail;
};

// TODO(mbkkt) try folly::IOBuf

}  // namespace sdb::basics
