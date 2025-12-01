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

#include "language.h"

#include <absl/strings/str_cat.h>

#include <cstdlib>

#include "app/app_server.h"
#include "app/global_context.h"
#include "app/options/option.h"
#include "app/options/parameters.h"
#include "app/options/program_options.h"
#include "basics/application-exit.h"
#include "basics/directories.h"
#include "basics/error.h"
#include "basics/exitcodes.h"
#include "basics/file_utils.h"
#include "basics/files.h"
#include "basics/logger/logger.h"

namespace sdb {

using namespace basics;
using namespace options;

namespace {

void SetCollator(const std::string& lang, void* icu_data_ptr,
                 LanguageType type) {
  switch (type) {
    case LanguageType::Default:
      SDB_DEBUG("xxxxx", Logger::CONFIG,
                "setting collator language for default to '", lang, "'");
      break;
    case LanguageType::ICU:
      SDB_DEBUG("xxxxx", Logger::CONFIG,
                "setting collator language for ICU to '", lang, "'");
      break;
    default:
      break;
  }

  if (!Utf8Helper::gDefault.setCollatorLanguage(lang, type, icu_data_ptr)) {
    SDB_FATAL_EXIT_CODE(
      "xxxxx", Logger::FIXME, EXIT_ICU_INITIALIZATION_FAILED,
      "error setting collator language to '", lang, "'. ",
      "The icudtl.dat file might be of the wrong version. ",
      "Check for an incorrectly set ICU_DATA environment variable");
  }
}

void SetLocale(icu::Locale& locale) {
  std::string language_name;

  if (!Utf8Helper::gDefault.getCollatorCountry().empty()) {
    language_name =
      absl::StrCat(Utf8Helper::gDefault.getCollatorLanguage(), "_",
                   Utf8Helper::gDefault.getCollatorCountry());
    locale = icu::Locale(Utf8Helper::gDefault.getCollatorLanguage().c_str(),
                         Utf8Helper::gDefault.getCollatorCountry().c_str()
                         /*
                            const   char * variant  = 0,
                            const   char * keywordsAndValues = 0
                         */
    );
  } else {
    locale = icu::Locale(Utf8Helper::gDefault.getCollatorLanguage().c_str());
    language_name = Utf8Helper::gDefault.getCollatorLanguage();
  }

  SDB_DEBUG("xxxxx", Logger::CONFIG, "using default language '", language_name,
            "'");
}

LanguageType GetLanguageType(std::string_view default_lang,
                             std::string_view icu_lang) noexcept {
  if (icu_lang.empty()) {
    return LanguageType::Default;
  }
  if (default_lang.empty()) {
    return LanguageType::ICU;
  }
  return LanguageType::Invalid;
}

}  // namespace

LanguageFeature::LanguageFeature(app::AppServer& server)
  : AppFeature{server, name()}, _binary_path{server.getBinaryPath()} {
  setOptional(false);
}

void LanguageFeature::collectOptions(
  std::shared_ptr<options::ProgramOptions> options) {
  options
    ->addOption("--default-language",
                "An ISO-639 language code. You can only set this option "
                "once, when initializing the database.",
                new StringParameter(&_default_language),
                options::MakeDefaultFlags(options::Flags::Uncommon))
    .setLongDescription(R"(The default language is used for sorting and
comparing strings. The language value is a two-letter language code (ISO-639) or
it is composed by a two-letter language code followed by a two letter country
code (ISO-3166). For example: `de`, `en`, `en_US`, `en_UK`.

The default is the system locale of the platform.)");

  options
    ->addOption(
      "--icu-language",
      "An ICU locale ID to set a language and optionally additional "
      "properties that affect string comparisons and sorting. You can only "
      "set this option once, when initializing the database.",
      new StringParameter(&_icu_language),
      options::MakeDefaultFlags(options::Flags::Uncommon))

    .setLongDescription(R"(With this option, you can get the sorting and
comparing order exactly as it is defined in the ICU standard. The language value
can be a two-letter language code (ISO-639), a two-letter language code followed
by a two letter country code (ISO-3166), or any other valid ICU locale
definition. For example: `de`, `en`, `en_US`, `en_UK`,
`de_AT@collation=phonebook`.

For the Swedish language (`sv`), for instance, the correct ICU-based sorting
order for letters is `'a','A','b','B','z','Z','å','Ä','ö','Ö'`. To get this
order, use `--icu-language sv`. If you use `--default-language sv` instead, the
sorting order will be `"A", "a", "B", "b", "Z", "z", "å", "Ä", "Ö", "ö"`.

**Note**: You can use only one of the language options, either `--icu-language`
or `--default-language`. Setting both of them results in an error.)");

  options->addOption(
    "--default-language-check",
    "Check if `--icu-language` / `--default-language` matches the "
    "stored language.",
    new BooleanParameter(&_force_language_check),
    options::MakeDefaultFlags(options::Flags::Uncommon));
}

std::string LanguageFeature::prepareIcu(
  const std::string& binary_path, const std::string& binary_execution_path,
  std::string& path, const std::string& binary_name) {
  std::string fn("icudtl.dat");
  if (SdbGETENV("ICU_DATA", path)) {
    path = file_utils::BuildFilename(path, fn);
  }
  if (path.empty() || !SdbIsRegularFile(path.c_str())) {
    if (!path.empty()) {
      SDB_WARN("xxxxx", Logger::FIXME, "failed to locate '", fn, "' at '", path,
               "'");
    }

    std::string bpfn = file_utils::BuildFilename(binary_execution_path, fn);

    if (SdbIsRegularFile(fn.c_str())) {
      path = fn;
    } else if (SdbIsRegularFile(bpfn.c_str())) {
      path = bpfn;
    } else {
      std::string argv0 =
        file_utils::BuildFilename(binary_execution_path, binary_name);
      path = SdbLocateInstallDirectory(argv0.c_str(), binary_path.c_str());
      path = file_utils::BuildFilename(path, ICU_DESTINATION_DIRECTORY, fn);

      if (!SdbIsRegularFile(path.c_str())) {
        // Try whether we have an absolute install prefix:
        path = file_utils::BuildFilename(ICU_DESTINATION_DIRECTORY, fn);
      }
    }

    if (!SdbIsRegularFile(path.c_str())) {
      std::string msg = absl::StrCat(
        "failed to initialize ICU library. Could not locate '", path,
        "'. Please make sure it is available. "
        "The environment variable ICU_DATA");
      std::string icupath;
      if (SdbGETENV("ICU_DATA", icupath)) {
        absl::StrAppend(&msg, "='", icupath, "'");
      }
      absl::StrAppend(&msg, " should point to the directory containing '", fn,
                      "'");

      SDB_DEBUG("xxxxx", Logger::FIXME, msg);
      return {};
    } else {
      std::string icu_path = path.substr(0, path.length() - fn.length());
      file_utils::MakePathAbsolute(icu_path);
      file_utils::NormalizePath(icu_path);
      setenv("ICU_DATA", icu_path.c_str(), 1);
    }
  }

  SDB_DEBUG("xxxxx", Logger::CONFIG, "loading ICU data from path '", path, "'");

  std::string icu_data = basics::file_utils::Slurp(path);

  if (icu_data.empty()) {
    SDB_INFO("xxxxx", Logger::FIXME, "failed to load '", fn, "' at '", path,
             "' - ", LastError());
  }

  return icu_data;
}

void LanguageFeature::prepare() {
  std::string p;
  auto context = GlobalContext::gContext;
  std::string binary_execution_path = context->getBinaryPath();
  std::string binary_name = context->binaryName();
  if (_icu_data.empty()) {
    _icu_data = LanguageFeature::prepareIcu(_binary_path, binary_execution_path,
                                            p, binary_name);
  }

  _lang_type = GetLanguageType(_default_language, _icu_language);

  if (LanguageType::Invalid == _lang_type) {
    SDB_FATAL("xxxxx", Logger::CONFIG,
              "Only one parameter from --default-language and --icu-language "
              "should be specified");
  }

  SetCollator(
    _lang_type == LanguageType::ICU ? _icu_language : _default_language,
    _icu_data.empty() ? nullptr : _icu_data.data(), _lang_type);
  SetLocale(_locale);
}

icu::Locale& LanguageFeature::getLocale() { return _locale; }

std::tuple<std::string_view, LanguageType> LanguageFeature::getLanguage()
  const {
  if (LanguageType::ICU == _lang_type) {
    return {_icu_language, _lang_type};
  }
  SDB_ASSERT(LanguageType::Default == _lang_type);
  // If it is invalid type just returning _default_language
  return {_default_language, _lang_type};
}

bool LanguageFeature::forceLanguageCheck() const {
  return _force_language_check;
}

std::string LanguageFeature::getCollatorLanguage() const {
  return Utf8Helper::gDefault.getCollatorLanguage();
}

void LanguageFeature::resetLanguage(std::string_view language,
                                    LanguageType type) {
  _lang_type = type;
  _default_language.clear();
  _icu_language.clear();
  switch (_lang_type) {
    case LanguageType::Default:
      _default_language = language;
      break;

    case LanguageType::ICU:
      _icu_language = language;
      break;

    case LanguageType::Invalid:
    default:
      SDB_ASSERT(false);
      return;
  }

  SetCollator(language.data(), _icu_data.empty() ? nullptr : _icu_data.data(),
              _lang_type);
  SetLocale(_locale);
}

}  // namespace sdb
