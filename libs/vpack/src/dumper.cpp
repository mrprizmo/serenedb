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

#include "vpack/dumper.h"

#include <cmath>

#include "basics/sink.h"
#include "basics/string_utils.h"
#include "vpack/common.h"
#include "vpack/exception.h"
#include "vpack/iterator.h"
#include "vpack/value_type.h"

namespace vpack {

// TODO(mbkkt) all reserve probably sucks and useless

template<typename Sink>
Dumper<Sink>::Dumper(Sink* sink, const Options* options)
  : sink{sink}, options{options} {
  if (sink == nullptr) [[unlikely]] {
    throw Exception(Exception::kInternalError, "Sink cannot be a nullptr");
  }
  if (options == nullptr) [[unlikely]] {
    throw Exception(Exception::kInternalError, "Options cannot be a nullptr");
  }
}

template<typename Sink>
void Dumper<Sink>::Dump(Slice slice) {
  // TODO(gnusi-vpack) reserve?
  _indentation = 0;
  DumpSlice(slice);
}

template<typename Sink>
void Dumper<Sink>::DumpStr(std::string_view str) {
  // TODO(gnusi-vpack) reserve from rapidjson?
  sink->PushChr('"');
  sdb::basics::string_utils::EscapeJsonStr(str, sink, *options);
  sink->PushChr('"');
}

template<typename Sink>
void Dumper<Sink>::DumpU64(uint64_t u) {
  sink->PushU64(u);
}

template<typename Sink>
void Dumper<Sink>::DumpI64(int64_t i) {
  sink->PushI64(i);
}

template<typename Sink>
void Dumper<Sink>::DumpSlice(Slice slice) {
  if (slice.isDouble()) {
    const auto d = slice.getDouble();  // TODO(gnusi) unchecked
    if (!options->unsupported_doubles_as_string && !std::isfinite(d)) {
      HandleUnsupportedType(slice);
    } else {
      sink->PushF64(d);
    }
  } else if (slice.isString()) {
    const auto str = slice.stringView();
    DumpStr(str);
  } else if (slice.isUInt()) {
    DumpU64(slice.getUIntUnchecked());
  } else if (slice.isNumber()) {
    DumpI64(slice.getIntUnchecked());
  } else if (slice.isNull()) {
    sink->PushStr("null");
  } else if (slice.isTrue()) {
    sink->PushStr("true");
  } else if (slice.isFalse()) {
    sink->PushStr("false");
  } else if (slice.isArray()) {
    ArrayIterator it(slice);
    sink->PushChr('[');
    if (options->pretty_print) {
      sink->PushChr('\n');
      ++_indentation;
      while (it.valid()) {
        Indent();
        DumpSlice(it.value());
        if (it.index() + 1 != it.size()) {
          sink->PushChr(',');
        }
        sink->PushChr('\n');
        it.next();
      }
      --_indentation;
      Indent();
    } else if (options->single_line_pretty_print) {
      while (it.valid()) {
        if (it.index() != 0) {
          sink->PushChr(',');
          sink->PushChr(' ');
        }
        DumpSlice(it.value());
        it.next();
      }
    } else {
      while (it.valid()) {
        if (it.index() != 0) {
          sink->PushChr(',');
        }
        DumpSlice(it.value());
        it.next();
      }
    }
    sink->PushChr(']');
  } else if (slice.isObject()) {
    ObjectIterator it(slice, !options->dump_attributes_in_index_order);
    sink->PushChr('{');
    if (options->pretty_print) {
      sink->PushChr('\n');
      ++_indentation;
      while (it.valid()) {
        auto current = (*it);
        Indent();
        DumpSlice(current.key);
        sink->PushStr(" : ");
        DumpSlice(current.value());
        if (it.index() + 1 != it.size()) {
          sink->PushChr(',');
        }
        sink->PushChr('\n');
        it.next();
      }
      --_indentation;
      Indent();
    } else if (options->single_line_pretty_print) {
      while (it.valid()) {
        if (it.index() != 0) {
          sink->PushChr(',');
          sink->PushChr(' ');
        }
        auto current = (*it);
        DumpSlice(current.key);
        sink->PushChr(':');
        sink->PushChr(' ');
        DumpSlice(current.value());
        it.next();
      }
    } else {
      while (it.valid()) {
        if (it.index() != 0) {
          sink->PushChr(',');
        }
        auto current = (*it);
        DumpSlice(current.key);
        sink->PushChr(':');
        DumpSlice(current.value());
        it.next();
      }
    }
    sink->PushChr('}');
  } else {
    HandleUnsupportedType(slice);
  }
}

template<typename Sink>
void Dumper<Sink>::Indent() {
  sink->PushSpaces(2 * _indentation);
}

template<typename Sink>
void Dumper<Sink>::HandleUnsupportedType(Slice slice) {
  if (options->unsupported_type_behavior == Options::kNullifyUnsupportedType) {
    sink->PushStr("null");
  } else if (options->unsupported_type_behavior ==
             Options::kConvertUnsupportedType) {
    sink->PushStr("\"(non-representable type ");
    sink->PushStr(slice.typeName());
    sink->PushStr(")\"");
  } else {
    throw Exception(Exception::kNoJsonEquivalent);
  }
}

template class Dumper<sdb::basics::LenSink>;
template class Dumper<sdb::basics::StrSink>;
template class Dumper<sdb::basics::CordSink>;

}  // namespace vpack
