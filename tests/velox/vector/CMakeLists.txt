################################################################################
### Velox Vector Tests
################################################################################

file(GLOB_RECURSE VELOX_VECTOR_TEST_SOURCES
    ${VELOX_SRC_DIR}/velox/vector/tests/*.cpp
)


# list(FILTER VELOX_VECTOR_TEST_SOURCES EXCLUDE REGEX ".*VectorSaverTest.cpp$")
# list(FILTER VELOX_VECTOR_TEST_SOURCES EXCLUDE REGEX ".*VectorTest.cpp$")
# list(FILTER VELOX_VECTOR_TEST_SOURCES EXCLUDE REGEX ".*VectorPrinterTest.cpp$")
# list(FILTER VELOX_VECTOR_TEST_SOURCES EXCLUDE REGEX ".*VectorStreamGroupTest.cpp$")
# list(FILTER VELOX_VECTOR_TEST_SOURCES EXCLUDE REGEX ".*EncodedVectorCopyTest.cpp$")

add_executable(velox_vector_test
    ${VELOX_VECTOR_TEST_SOURCES}
)

add_library(velox_temp_path 
    ${VELOX_SRC_DIR}/velox/exec/tests/utils/TempFilePath.cpp 
    ${VELOX_SRC_DIR}/velox/exec/tests/utils/TempDirectoryPath.cpp
)
target_link_libraries(velox_temp_path PRIVATE velox_exception)


target_link_libraries(
    velox_vector_test
    PRIVATE
        velox_vector_fuzzer
        velox_time
        velox_vector_test_lib
        velox_buffer
        velox_serialization
        velox_presto_serializer
        velox_presto_types
        velox_temp_path
        velox_type_test_lib
        velox_common_fuzzer_util
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        Folly::folly
        glog::glog
        fmt::fmt
        dl
)

add_compile_definitions(VELOX_ENABLE_FUZZER)

# vector fuzzer tests
add_subdirectory(
    ${VELOX_SRC_DIR}/velox/vector/fuzzer/tests
    ${CMAKE_CURRENT_BINARY_DIR}/velox_vector_fuzzer_tests
)
