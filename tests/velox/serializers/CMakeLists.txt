################################################################################
### Velox Serializers Tests
################################################################################

set(VELOX_SERIALIZER_TESTS ${VELOX_SRC_DIR}/velox/serializers/tests)

add_executable(
    velox_serializer_test
    ${VELOX_SERIALIZER_TESTS}/PrestoOutputStreamListenerTest.cpp
    # It needed compression in folly. 
    ${VELOX_SERIALIZER_TESTS}/PrestoSerializerTest.cpp
)

add_test(velox_serializer_test velox_serializer_test)

target_link_libraries(
    velox_serializer_test
    velox_presto_serializer
    velox_vector_test_lib
    velox_vector_fuzzer
    velox_row_fast
    GTest::gtest
    GTest::gtest_main
    glog::glog
)
