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

#include "vpack/iterator.h"

#include <ostream>

#include "vpack/common.h"

using namespace vpack;

std::ostream& operator<<(std::ostream& stream, const ArrayIterator* it) {
  stream << "[ArrayIterator " << it->index() << " / " << it->size() << "]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const ArrayIterator& it) {
  return operator<<(stream, &it);
}

std::ostream& operator<<(std::ostream& stream, const ObjectIterator* it) {
  stream << "[ObjectIterator " << it->index() << " / " << it->size() << "]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const ObjectIterator& it) {
  return operator<<(stream, &it);
}
