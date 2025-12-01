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

#include <vpack/builder.h>
#include <vpack/slice.h>
#include <vpack/validation_types.h>

#include <string>
#include <tao/json/forward.hpp>

#include "basics/result.h"
#include "basics/result_or.h"

namespace tao::json {

template<template<typename...> typename Traits>
class basic_schema;  // NOLINT

}  // namespace tao::json
namespace sdb {

enum class ValidationLevel {
  None = 0,
  New = 1,
  Moderate = 2,
  Strict = 3,
};

class ValidatorBase {
 public:
  static bool isSame(vpack::Slice validator1, vpack::Slice validator2);

  explicit ValidatorBase(vpack::Slice params);
  virtual ~ValidatorBase() = default;

  // Validation function as it should be used in the logical collection or
  // storage engine.
  virtual Result validate(vpack::Slice new_doc, vpack::Slice old_doc,
                          bool is_insert, const vpack::Options*) const;

  // Validate a single document in the specialized class ignoring the level.
  // This version is used in the implementations of AQL Functions.
  virtual Result validateOne(vpack::Slice slice,
                             const vpack::Options*) const = 0;

  void toVPack(vpack::Builder&) const;
  virtual std::string_view type() const = 0;
  const std::string& message() const { return this->_message; }
  const std::string& specialProperties() const;
  void setLevel(ValidationLevel level) noexcept { _level = level; }
  ValidationLevel level() { return _level; }

 protected:
  ValidatorBase() = default;
  virtual void toVPackDerived(vpack::Builder&) const = 0;

  std::string _message;
  ValidationLevel _level = ValidationLevel::Strict;
  vpack::validation::SpecialProperties _special =
    vpack::validation::SpecialProperties::None;
};

class ValidatorJsonSchema final : public ValidatorBase {
 public:
  static ResultOr<std::shared_ptr<ValidatorJsonSchema>> buildInstance(
    vpack::Slice schema);

  explicit ValidatorJsonSchema(vpack::Slice params);

  std::string_view type() const override { return "json"; }
  Result validateOne(vpack::Slice slice, const vpack::Options*) const override;
  void toVPackDerived(vpack::Builder& b) const override;

 private:
  std::shared_ptr<tao::json::basic_schema<tao::json::traits>> _schema;
  vpack::Builder _builder;
};

template<typename Context>
void VPackRead(Context ctx, std::shared_ptr<ValidatorBase>& values) {
  auto r = ValidatorJsonSchema::buildInstance(ctx.vpack());
  if (!r) {
    SDB_THROW(std::move(r).error());
  }
  values = std::move(*r);
}

void VPackWrite(auto ctx, const ValidatorBase& schema) {
  schema.toVPack(ctx.vpack());
}

}  // namespace sdb
