#pragma once
#include <string>

namespace sdb::pg::functions {

void RegisterUnionFunctions(const std::string& prefix);

}  // namespace sdb::pg::functions
