################################################################################
### Velox Union Functions Tests
################################################################################

add_executable(velox_union_functions_test
    UnionFunctionsTest.cpp
    ${CMAKE_SOURCE_DIR}/server/pg/functions/union.cpp
    ${CMAKE_SOURCE_DIR}/server/query/types.cpp
)

target_include_directories(velox_union_functions_test
    PRIVATE
        ${VELOX_SRC_DIR}
        ${CMAKE_SOURCE_DIR}/server
        ${CMAKE_SOURCE_DIR}/libs
)

target_link_libraries(
    velox_union_functions_test
    PRIVATE
        velox_functions_prestosql_impl
        velox_functions_prestosql
        velox_expression
        velox_vector_test_lib
        velox_vector
        velox_type
        velox_memory
        velox_presto_types
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        absl::flat_hash_map
        Folly::folly
        glog::glog
        fmt::fmt
)
