set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/")

if(SDB_GTEST)
 add_subdirectory(velox)
endif()

add_compile_warnings_flags()

add_subdirectory(bench)

if(SDB_GTEST)
    add_subdirectory(libs)
    add_subdirectory(server)
endif()
