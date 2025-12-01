# - Find LZ4 (lz4.h, liblz4.a, liblz4.so, and liblz4.so.1)
# This module defines
#  LZ4_INCLUDE_DIR, directory containing headers
#  LZ4_LIBRARY_DIR, directory containing lz4 libraries
#  LZ4_STATIC_LIB, path to liblz4.lib
#  LZ4_FOUND, whether lz4 has been found

if("${LZ4_ROOT}" STREQUAL "")
    set(LZ4_ROOT "$ENV{LZ4_ROOT}")
    if(NOT "${LZ4_ROOT}" STREQUAL "")
        string(REPLACE "\"" "" LZ4_ROOT ${LZ4_ROOT})
    endif()
endif()

if(NOT "${LZ4_ROOT}" STREQUAL "")
    set(LZ4_SEARCH_HEADER_PATHS
        ${LZ4_ROOT}
        ${LZ4_ROOT}/include
        ${LZ4_ROOT}/include/lz4
        ${LZ4_ROOT}/include
        ${LZ4_ROOT}/lib
    )

    set(LZ4_SEARCH_LIB_PATHS ${LZ4_ROOT} ${LZ4_ROOT}/lib)

    set(LZ4_SEARCH_SRC_PATHS
        ${LZ4_ROOT}
        ${LZ4_ROOT}/lib
        ${LZ4_ROOT}/src
        ${LZ4_ROOT}/src/lz4
        ${LZ4_ROOT}/src/lz4/lib
    )
elseif(NOT MSVC)
    set(LZ4_SEARCH_HEADER_PATHS
        "/usr/include"
        "/usr/include/lz4"
        "/usr/include/x86_64-linux-gnu"
        "/usr/include/x86_64-linux-gnu/lz4"
    )

    set(LZ4_SEARCH_LIB_PATHS
        "/lib"
        "/lib/x86_64-linux-gnu"
        "/usr/lib"
        "/usr/lib/x86_64-linux-gnu"
    )

    set(LZ4_SEARCH_SRC_PATHS "/usr/src" "/usr/src/lz4" "/usr/src/lz4/lib")
endif()

find_path(
    LZ4_INCLUDE_DIR
    lz4.h
    PATHS ${LZ4_SEARCH_HEADER_PATHS}
    NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

find_path(
    LZ4_SRC_DIR_LZ4
    lz4.c
    PATHS ${LZ4_SEARCH_SRC_PATHS}
    NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
)

if(LZ4_SRC_DIR_LZ4)
    get_filename_component(LZ4_SRC_DIR_PARENT ${LZ4_SRC_DIR_LZ4} DIRECTORY)

    set(LZ4_CMAKE_PATH "")

    if(IS_DIRECTORY "${LZ4_SRC_DIR_PARENT}/build/cmake")
        if(EXISTS "${LZ4_SRC_DIR_PARENT}/build/cmake/CMakeLists.txt") # Location of CMakeLists.txt is changed since 1.9.3
            set(LZ4_CMAKE_PATH ${LZ4_SRC_DIR_PARENT}/build/cmake)
        endif()
    elseif(IS_DIRECTORY "${LZ4_SRC_DIR_PARENT}/contrib/cmake_unofficial")
        if(
            EXISTS
                "${LZ4_SRC_DIR_PARENT}/contrib/cmake_unofficial/CMakeLists.txt"
        ) # Location of CMakeLists.txt before 1.9.3
            set(LZ4_CMAKE_PATH ${LZ4_SRC_DIR_PARENT}/contrib/cmake_unofficial)
        endif()
    endif()

    if("${LZ4_CMAKE_PATH}" STREQUAL "")
        message(FATAL_ERROR "Can't find CMakeLists.txt for LZ4")
    endif()

    find_path(
        LZ4_SRC_DIR_CMAKE
        CMakeLists.txt
        PATHS ${LZ4_SRC_DIR_LZ4} ${LZ4_CMAKE_PATH}
        NO_DEFAULT_PATH # make sure we don't accidentally pick up a different version
    )
endif()

# found the cmake enabled source directory
if(LZ4_INCLUDE_DIR AND LZ4_SRC_DIR_LZ4 AND LZ4_SRC_DIR_CMAKE)
    set(LZ4_FOUND TRUE)
    set(LZ4_BUILD_CLI OFF)
    set(LZ4_BUILD_LEGACY_LZ4C OFF)
    set(LZ4_BUNDLED_MODE ON)
    set(LZ4_POSITION_INDEPENDENT_LIB OFF)

    add_subdirectory(
        ${LZ4_SRC_DIR_CMAKE}
        ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-lz4.dir
        EXCLUDE_FROM_ALL # do not build unused targets
    )

    set(LZ4_LIBRARY_DIR ${LZ4_SEARCH_LIB_PATHS})
    set(LZ4_STATIC_LIB lz4_static)
    target_include_directories(lz4_static INTERFACE ${LZ4_INCLUDE_DIR})

    return()
endif()

include(Utils)

# set options for: static
if(MSVC)
    set(LZ4_LIBRARY_PREFIX "")
    set(LZ4_LIBRARY_SUFFIX ".lib")
else()
    set(LZ4_LIBRARY_PREFIX "lib")
    set(LZ4_LIBRARY_SUFFIX ".a")
endif()
set_find_library_options("${LZ4_LIBRARY_PREFIX}" "${LZ4_LIBRARY_SUFFIX}")

# find library
find_library(
    LZ4_STATIC_LIB
    NAMES lz4
    PATHS ${LZ4_SEARCH_LIB_PATHS}
    NO_DEFAULT_PATH
)

# restore initial options
restore_find_library_options()

if(LZ4_INCLUDE_DIR AND LZ4_STATIC_LIB)
    set(LZ4_FOUND TRUE)
    set(LZ4_LIBRARY_DIR
        "${LZ4_SEARCH_LIB_PATHS}"
        CACHE PATH
        "Directory containing lz4 libraries"
        FORCE
    )

    add_library(lz4_static IMPORTED STATIC)
    set_target_properties(
        lz4_static
        PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LZ4_INCLUDE_DIR}"
            IMPORTED_LOCATION "${LZ4_STATIC_LIB}"
    )
else()
    set(LZ4_FOUND FALSE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    LZ4
    DEFAULT_MSG
    LZ4_INCLUDE_DIR
    LZ4_STATIC_LIB
)
message("LZ4_INCLUDE_DIR: " ${LZ4_INCLUDE_DIR})
message("LZ4_LIBRARY_DIR: " ${LZ4_LIBRARY_DIR})
message("LZ4_STATIC_LIB: " ${LZ4_STATIC_LIB})

mark_as_advanced(LZ4_INCLUDE_DIR LZ4_LIBRARY_DIR LZ4_STATIC_LIB)
