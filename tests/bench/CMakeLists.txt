add_subdirectory(micro)

add_executable(serenedb-bench-vpack vpack.cpp)
target_link_libraries(serenedb-bench-vpack PRIVATE vpack simdjson rapidjson)

add_executable(serenedb-union-vs-row-bench union_vs_row.cpp)
target_link_libraries(serenedb-union-vs-row-bench PRIVATE
    sereneserver
    benchmark
    velox_vector_test_lib
    velox_expression
    velox_functions_prestosql
    velox_functions_prestosql_impl
)
target_include_directories(serenedb-union-vs-row-bench PRIVATE
    ${CMAKE_SOURCE_DIR}/server
    ${CMAKE_SOURCE_DIR}/third_party/velox
)
