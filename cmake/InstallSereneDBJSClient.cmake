################################################################################
### @brief install client-side JavaScript files
################################################################################

install(
    DIRECTORY
        ${SERENEDB_SOURCE_DIR}/private/js/common
        ${SERENEDB_SOURCE_DIR}/private/js/client
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_SERENE}/${SERENEDB_JS_VERSION}
    FILES_MATCHING
    PATTERN "*.js"
    REGEX "^.*/js/client/modules/@serenedb/testsuites" EXCLUDE
    REGEX "^.*/js/client/modules/@serenedb/testutils" EXCLUDE
)

install(
    FILES ${SERENEDB_SOURCE_DIR}/private/js/JS_SHA1SUM.txt
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_SERENE}/${SERENEDB_JS_VERSION}
)

# For the node modules we need all files except the following:
install(
    DIRECTORY ${SERENEDB_SOURCE_DIR}/private/js/node
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_SERENE}/${SERENEDB_JS_VERSION}
    REGEX "^.*/ansi_up" EXCLUDE
    REGEX "^.*/eslint" EXCLUDE
    REGEX "^.*/node-netstat" EXCLUDE
    REGEX "^.*/parse-prometheus-text-format" EXCLUDE
    REGEX "^.*/@xmldom" EXCLUDE
    REGEX "^.*/.bin" EXCLUDE
    REGEX "^.*/.npmignore" EXCLUDE
    REGEX "^.*/.*-no-eslint" EXCLUDE
    REGEX "^.*js/node/package.*.json" EXCLUDE
)

install(
    FILES ${SERENEDB_SOURCE_DIR}/private/js/node/package.json-no-eslint
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_SERENE}/${SERENEDB_JS_VERSION}/node
    RENAME package.json
)

install(
    FILES ${SERENEDB_SOURCE_DIR}/private/js/node/package-lock.json-no-eslint
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR_SERENE}/${SERENEDB_JS_VERSION}/node
    RENAME package-lock.json
)
