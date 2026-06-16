# Unit tests
add_executable(unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/test_handle.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_connection_string.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_type_conversion.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_json_parse.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_catalog_write.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_transaction_batch.c
)

target_link_libraries(unit_tests PRIVATE trino_odbc CURL::libcurl Threads::Threads)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME unit_tests COMMAND unit_tests)

# Integration tests (can run without live Trino instance for basic tests)
add_executable(integration_tests
    ${CMAKE_SOURCE_DIR}/test/integration/test_param_binding.c
)

target_link_libraries(integration_tests PRIVATE trino_odbc CURL::libcurl Threads::Threads)
target_include_directories(integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME integration_tests COMMAND integration_tests)
set_tests_properties(integration_tests PROPERTIES LABELS "integration")

# End-to-end tests (full execute/fetch/getdata path via a mock HTTP transport)
add_executable(e2e_tests
    ${CMAKE_SOURCE_DIR}/test/integration/test_e2e_query.c
)

target_link_libraries(e2e_tests PRIVATE trino_odbc CURL::libcurl Threads::Threads)
target_include_directories(e2e_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME e2e_tests COMMAND e2e_tests)
set_tests_properties(e2e_tests PROPERTIES LABELS "integration")

# Installation targets
install(TARGETS trino_odbc
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    RUNTIME DESTINATION bin
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)

# ODBC driver configuration
configure_file(
    ${CMAKE_SOURCE_DIR}/trino_odbc.odbcinst.ini.in
    ${CMAKE_BINARY_DIR}/trino_odbc.odbcinst.ini
    @ONLY
)

install(FILES ${CMAKE_BINARY_DIR}/trino_odbc.odbcinst.ini
    DESTINATION etc
    RENAME odbcinst.ini
)
