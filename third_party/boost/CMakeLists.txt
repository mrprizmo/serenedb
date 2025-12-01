project(boost)

set(boost_version "1.87.0")
set(BOOST_VERSION ${boost_version} PARENT_SCOPE)
set(boost_src "${CMAKE_CURRENT_SOURCE_DIR}")

if (MSVC)
  add_definitions(-DBOOST_THREAD_BUILD_LIB)
endif()

add_c_flags_if_supported(CMAKE_C_FLAGS -Wno-suggest-override)
add_cxx_flags_if_supported(CMAKE_CXX_FLAGS -Wno-suggest-override)

message(STATUS "using third_party BOOST")
add_library(boost_boost INTERFACE)
target_include_directories(boost_boost SYSTEM INTERFACE "${boost_src}")
target_compile_definitions(boost_boost INTERFACE
  "BOOST_NO_CXX98_FUNCTION_BASE=1"
  "BOOST_UUID_RANDOM_PROVIDER_FORCE_POSIX=1"
  "BOOST_ALL_NO_LIB=1"
  "BOOST_SYSTEM_STATIC_LINK=1"
  "BOOST_THREAD_DONT_PROVIDE_INTERRUPTIONS=1"
  "BOOST_PFR_USE_STD_MAKE_INTEGRAL_SEQUENCE=1"
  "BOOST_ASIO_NO_DEPRECATED=1"
  "_LIBCPP_HAS_ALIGNED_ALLOC=1"
)
