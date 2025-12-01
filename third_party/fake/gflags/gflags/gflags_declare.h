#pragma once

#define FAKE_GFLAGS 1

#include <absl/flags/declare.h>
#include <string>

namespace gflags {

template<typename T>
struct Flag;

}  // namespace gflags

#define GFLAGS_DECLARE_FLAG_INTERNAL(type, name)             \
  extern gflags::Flag<type> FLAGS_##name;                    \
  namespace gflags /* block flags in namespaces */ {}        \
  /* second redeclaration is to allow applying attributes */ \
  extern gflags::Flag<type> FLAGS_##name

#define DECLARE_bool(name) GFLAGS_DECLARE_FLAG_INTERNAL(bool, name)
#define DECLARE_int32(name) GFLAGS_DECLARE_FLAG_INTERNAL(int32_t, name)
#define DECLARE_string(name) GFLAGS_DECLARE_FLAG_INTERNAL(std::string, name)
