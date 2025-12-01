#pragma once

#include <string_view>
#include <utility>

namespace vpack::validation {

enum class SpecialProperties {
  None = 0,
  Key = 1,
  Id = 2,
  Rev = 4,
  From = 8,
  To = 16,
  All = 31,
};

[[nodiscard]] SpecialProperties StrToSpecial(std::string_view str);
[[nodiscard]] bool SkipSpecial(std::string_view str, SpecialProperties special);

}  // namespace vpack::validation
