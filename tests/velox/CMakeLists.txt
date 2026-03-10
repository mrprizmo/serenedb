################################################################################
### Velox tests integration
################################################################################

set(VELOX_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/velox)

include_directories(
    ${VELOX_SRC_DIR}
)

add_subdirectory(type)
add_subdirectory(vector)
