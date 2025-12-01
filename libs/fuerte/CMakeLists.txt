add_library(
    fuerte
    STATIC
    src/connection.cpp
    src/connection_builder.cpp
    src/helper.cpp
    src/http.cpp
    src/h1_connection.cpp
    src/h2_connection.cpp
    src/jwt.cpp
    src/loop.cpp
    src/message.cpp
    src/requests.cpp
    src/types.cpp
    src/connection.cpp
)

# TODO: This is not enough to link fuerte on its own
target_link_libraries(
    fuerte
    PUBLIC nghttp2::nghttp2 llhttp boost_boost absl::strings vpack serene_base
    PRIVATE yaclib
)

target_include_directories(
    fuerte
    PUBLIC src
    SYSTEM
    PUBLIC include "${OPENSSL_INCLUDE_DIR}"
)
