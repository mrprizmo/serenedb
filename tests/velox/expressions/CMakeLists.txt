################################################################################
### Velox Expression Tests (Union Cast)
################################################################################

add_executable(velox_union_cast_test
    UnionCastTest.cpp
)

target_include_directories(velox_union_cast_test
    PRIVATE
        ${VELOX_SRC_DIR}
)

target_link_libraries(
    velox_union_cast_test
    PRIVATE
        velox_expression
        velox_vector_test_lib
        velox_vector
        velox_type
        velox_memory
        velox_presto_types
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        Folly::folly
        glog::glog
        fmt::fmt
)

add_executable(velox_union_comparison_test
    UnionComparisonTest.cpp
)

target_include_directories(velox_union_comparison_test
    PRIVATE
        ${VELOX_SRC_DIR}
)

target_link_libraries(
    velox_union_comparison_test
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
        Folly::folly
        glog::glog
        fmt::fmt
)
