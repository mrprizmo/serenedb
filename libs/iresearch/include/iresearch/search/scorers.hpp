////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "iresearch/search/scorer.hpp"
#include "iresearch/utils/text_format.hpp"

namespace irs {

class ScorerRegistrar {
 public:
  ScorerRegistrar(const TypeInfo& type, const TypeInfo& args_format,
                  Scorer::ptr (*factory)(std::string_view args),
                  const char* source = nullptr);
  explicit operator bool() const noexcept { return _registered; }

 private:
  bool _registered;
};

#define REGISTER_SCORER_IMPL(scorer_name, args_format, factory, line, source) \
  static ::irs::ScorerRegistrar scorer_registrar##_##line(                    \
    ::irs::Type<scorer_name>::get(), ::irs::Type<args_format>::get(),         \
    &factory, source)
#define REGISTER_SCORER_EXPANDER(scorer_name, args_format, factory, file, \
                                 line)                                    \
  REGISTER_SCORER_IMPL(scorer_name, args_format, factory, line,           \
                       file ":" IRS_TO_STRING(line))
#define REGISTER_SCORER(scorer_name, args_format, factory)              \
  REGISTER_SCORER_EXPANDER(scorer_name, args_format, factory, __FILE__, \
                           __LINE__)
#define REGISTER_SCORER_JSON(scorer_name, factory) \
  REGISTER_SCORER(scorer_name, ::irs::text_format::Json, factory)
#define REGISTER_SCORER_VPACK(scorer_name, factory) \
  REGISTER_SCORER(scorer_name, ::irs::text_format::VPack, factory)

namespace scorers {

// Check whether scorer with a specified name is registered
bool Exists(std::string_view name, const TypeInfo& args_format,
            bool load_library = true);

// Find a scorer by name, or nullptr if not found
// indirect call to <class>::make(...)
// NOTE: make(...) MUST be defined in CPP to ensire proper code scope
Scorer::ptr Get(std::string_view name, const TypeInfo& args_format,
                std::string_view args, bool load_library = true) noexcept;

// For static lib reference all known scorers in lib
// for shared lib NOOP
// no explicit call of fn is required, existence of fn is sufficient
void Init();

// Load all scorers from plugins directory
void LoadAll(std::string_view path);

// Visit all loaded scorers, terminate early if visitor returns false
bool Visit(
  const std::function<bool(std::string_view, const TypeInfo&)>& visitor);

}  // namespace scorers
}  // namespace irs
