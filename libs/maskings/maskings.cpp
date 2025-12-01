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

#include "maskings.h"

#include <absl/strings/str_cat.h>
#include <vpack/builder.h>
#include <vpack/exception.h>
#include <vpack/iterator.h>
#include <vpack/options.h>
#include <vpack/parser.h>
#include <vpack/slice.h>

#include <cstdint>
#include <iostream>
#include <string_view>

#include "basics/debugging.h"
#include "basics/file_utils.h"
#include "basics/logger/logger.h"
#include "basics/random/random_generator.h"
#include "basics/static_strings.h"
#include "maskings/collection_selection.h"
#include "maskings/masking_function.h"

using namespace sdb;
using namespace sdb::maskings;

MaskingsResult Maskings::fromFile(const std::string& filename) {
  std::string definition;

  try {
    definition = basics::file_utils::Slurp(filename);
  } catch (const std::exception& e) {
    auto msg =
      absl::StrCat("cannot read maskings file '", filename, "': ", e.what());
    SDB_DEBUG("xxxxx", Logger::CONFIG, msg);

    return MaskingsResult(MaskingsResult::kCannotReadFile, std::move(msg));
  }

  SDB_DEBUG("xxxxx", Logger::CONFIG, "found maskings file '", filename);

  if (definition.empty()) {
    auto msg = absl::StrCat("maskings file '", filename, "' is empty");
    SDB_DEBUG("xxxxx", Logger::CONFIG, msg);
    return MaskingsResult(MaskingsResult::kCannotReadFile, std::move(msg));
  }

  auto maskings = std::make_unique<Maskings>();

  maskings.get()->_random_seed = random::RandU64();

  try {
    std::shared_ptr<vpack::Builder> parsed =
      vpack::Parser::fromJson(definition);

    ParseResult<Maskings> res = maskings->parse(parsed->slice());

    if (res.status != ParseResult<Maskings>::kValid) {
      return MaskingsResult(MaskingsResult::kIllegalDefinition, res.message);
    }

    return MaskingsResult(std::move(maskings));
  } catch (const vpack::Exception& e) {
    auto msg =
      absl::StrCat("cannot parse maskings file '", filename, "': ", e.what());
    SDB_DEBUG("xxxxx", Logger::CONFIG, msg, ". file content: ", definition);

    return MaskingsResult(MaskingsResult::kCannotParseFile, std::move(msg));
  }
}

ParseResult<Maskings> Maskings::parse(vpack::Slice def) {
  if (!def.isObject()) {
    return ParseResult<Maskings>(ParseResult<Maskings>::kDuplicateCollection,
                                 "expecting an object for masking definition");
  }

  auto it = _collections.end();
  for (const auto& entry : vpack::ObjectIterator(def, false)) {
    auto key = entry.key.stringView();

    if (key == "*") {
      SDB_TRACE("xxxxx", Logger::CONFIG, "default masking");

      if (_has_default_collection) {
        return ParseResult<Maskings>(
          ParseResult<Maskings>::kDuplicateCollection,
          "duplicate default entry");
      }
    } else {
      SDB_TRACE("xxxxx", Logger::CONFIG, "masking collection '", key, "'");

      it = _collections.lower_bound(key);
      if (it != _collections.end() && it->first == key) {
        return ParseResult<Maskings>(
          ParseResult<Maskings>::kDuplicateCollection,
          absl::StrCat("duplicate collection entry '", key, "'"));
      }
    }

    ParseResult<Collection> c = Collection::parse(this, entry.value());

    if (c.status != ParseResult<Collection>::kValid) {
      return ParseResult<Maskings>(
        (ParseResult<Maskings>::StatusCode)(int)c.status, c.message);
    }

    if (key == "*") {
      _has_default_collection = true;
      _default_collection = c.result;
    } else {
      _collections.emplace_hint(it, key, c.result);
    }
  }

  return ParseResult<Maskings>(ParseResult<Maskings>::kValid);
}

bool Maskings::shouldDumpStructure(const std::string& name) {
  CollectionSelection select = CollectionSelection::EXCLUDE;
  const auto itr = _collections.find(name);

  if (itr == _collections.end()) {
    if (_has_default_collection) {
      select = _default_collection.selection();
    }
  } else {
    select = itr->second.selection();
  }

  switch (select) {
    case CollectionSelection::FULL:
      return true;
    case CollectionSelection::MASKED:
      return true;
    case CollectionSelection::EXCLUDE:
      return false;
    case CollectionSelection::STRUCTURE:
      return true;
  }

  // should not get here. however, compiler warns about it
  SDB_ASSERT(false);
  return false;
}

bool Maskings::shouldDumpData(const std::string& name) {
  CollectionSelection select = CollectionSelection::EXCLUDE;
  const auto itr = _collections.find(name);

  if (itr == _collections.end()) {
    if (_has_default_collection) {
      select = _default_collection.selection();
    }
  } else {
    select = itr->second.selection();
  }

  switch (select) {
    case CollectionSelection::FULL:
      return true;
    case CollectionSelection::MASKED:
      return true;
    case CollectionSelection::EXCLUDE:
      return false;
    case CollectionSelection::STRUCTURE:
      return false;
  }

  // should not get here. however, compiler warns about it
  SDB_ASSERT(false);
  return false;
}

void Maskings::maskedItem(const Collection& collection,
                          std::vector<std::string_view>& path,
                          vpack::Slice data, vpack::Builder& out,
                          std::string& buffer) const {
  if (path.size() == 1 && path[0].starts_with('_')) {
    if (data.isString()) {
      out.add(data);
      return;
    } else if (data.isInteger()) {
      out.add(data);
      return;
    }
  }

  MaskingFunction* func = collection.masking(path);

  if (func == nullptr) {
    if (data.isBool() || data.isString() || data.isInteger() ||
        data.isDouble()) {
      out.add(data);
      return;
    }
  } else {
    if (data.isBool()) {
      func->mask(data.getBool(), out, buffer);
      return;
    } else if (data.isString()) {
      func->mask(data.stringView(), out, buffer);
      return;
    } else if (data.isInteger()) {
      func->mask(data.getInt(), out, buffer);
      return;
    } else if (data.isDouble()) {
      func->mask(data.getDouble(), out, buffer);
      return;
    }
  }

  out.add(vpack::Value(vpack::ValueType::Null));
}

void Maskings::addMaskedArray(const Collection& collection,
                              std::vector<std::string_view>& path,
                              vpack::Slice data, vpack::Builder& out,
                              std::string& buffer) const {
  for (vpack::Slice value : vpack::ArrayIterator(data)) {
    if (value.isObject()) {
      vpack::ObjectBuilder ob(&out);
      addMaskedObject(collection, path, value, out, buffer);
    } else if (value.isArray()) {
      vpack::ArrayBuilder ap(&out);
      addMaskedArray(collection, path, value, out, buffer);
    } else {
      maskedItem(collection, path, value, out, buffer);
    }
  }
}

void Maskings::addMaskedObject(const Collection& collection,
                               std::vector<std::string_view>& path,
                               vpack::Slice data, vpack::Builder& out,
                               std::string& buffer) const {
  for (auto entry : vpack::ObjectIterator(data, false)) {
    auto key = entry.key.stringView();
    vpack::Slice value = entry.value();

    path.push_back(key);

    if (value.isObject()) {
      vpack::ObjectBuilder ob(&out, key);
      addMaskedObject(collection, path, value, out, buffer);
    } else if (value.isArray()) {
      vpack::ArrayBuilder ap(&out, key);
      addMaskedArray(collection, path, value, out, buffer);
    } else {
      out.add(key);
      maskedItem(collection, path, value, out, buffer);
    }

    path.pop_back();
  }
}

void Maskings::addMasked(const Collection& collection, vpack::Builder& out,
                         vpack::Slice data) const {
  if (!data.isObject()) {
    return;
  }

  std::string buffer;
  std::vector<std::string_view> path;

  out.openObject();
  addMaskedObject(collection, path, data, out, buffer);
  out.close();
}

void Maskings::mask(const std::string& name, vpack::Slice data,
                    vpack::Builder& builder) const {
  const Collection* collection;
  const auto itr = _collections.find(name);

  if (itr == _collections.end()) {
    if (_has_default_collection) {
      collection = &_default_collection;
    } else {
      builder.add(data);
      return;
    }
  } else {
    collection = &(itr->second);
  }

  if (collection->selection() == CollectionSelection::FULL) {
    builder.add(data);
    return;
  }

  addMasked(*collection, builder, data);
}
