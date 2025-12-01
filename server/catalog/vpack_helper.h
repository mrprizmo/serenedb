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

#include <vpack/slice.h>

#include <iresearch/utils/string.hpp>  // for std::string_view

#include "basics/common.h"
#include "basics/debugging.h"

namespace vpack {

class Builder;

}  // namespace vpack
namespace sdb::search {

// according to Slice.h:330
const uint8_t kCompactArray = 0x13;
const uint8_t kCompactObject = 0x14;

template<typename Chr>
auto ToStr(vpack::Slice s) {
  static_assert(sizeof(Chr) == sizeof(uint8_t));
  return irs::basic_string_view{s.startAs<Chr>(), s.byteSize()};
}

template<typename Chr>
auto ToSlice(irs::basic_string_view<Chr> s) {
  static_assert(sizeof(Chr) == sizeof(uint8_t));
  SDB_ASSERT(s.data());
  return vpack::Slice{reinterpret_cast<const uint8_t*>(s.data())};
}

// TODO(mbkkt) remove these "ref" functions

vpack::Builder& AddStringRef(vpack::Builder& builder, std::string_view value);

vpack::Builder& AddStringRef(vpack::Builder& builder, std::string_view key,
                             std::string_view value);

inline std::string_view GetStringRef(vpack::Slice slice) {
  if (slice.isNull()) {
    return {};
  }
  SDB_ASSERT(slice.isString());
  return slice.stringView();
}

inline irs::bytes_view GetBytesRef(vpack::Slice slice) {
  SDB_ASSERT(slice.isString());
  return irs::ViewCast<irs::byte_type>(slice.stringView());
}
template<typename T>
inline bool GetNumber(T& buf, vpack::Slice slice) noexcept {
  if (!slice.isNumber()) {
    return false;
  }

  using NumType = std::conditional_t<std::is_floating_point_v<T>, T, double>;

  try {
    auto value = slice.getNumber<NumType>();

    buf = static_cast<T>(value);

    return value == static_cast<decltype(value)>(buf);
  } catch (...) {
    // NOOP
  }

  return false;
}

template<typename T>
inline bool GetNumber(T& buf, vpack::Slice slice, std::string_view field_name,
                      bool& seen, T fallback) noexcept {
  auto field = slice.get(field_name);
  seen = !field.isNone();
  if (!seen) {
    buf = fallback;
    return true;
  }
  return getNumber(buf, field);
}

template<typename T>
vpack::Slice GetSlice(vpack::Slice slice, const T& attribute_path,
                      vpack::Slice fallback = vpack::Slice::nullSlice()) {
  if (attribute_path.empty()) {
    return fallback;
  }

  for (size_t i = 0, size = attribute_path.size(); i < size; ++i) {
    slice = slice.get(attribute_path[i].name);

    if (slice.isNone() || (i + 1 < size && !slice.isObject())) {
      return fallback;
    }
  }

  return slice;
}

bool ParseDirectionBool(vpack::Slice slice, bool& direction);

bool ParseDirectionString(vpack::Slice slice, bool& direction);

}  // namespace sdb::search
