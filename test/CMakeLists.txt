# Unit tests
add_executable(unit_tests
    ${CMAKE_SOURCE_DIR}/test/unit/test_handle.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_connection_string.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_type_conversion.c
    ${CMAKE_SOURCE_DIR}/test/unit/test_json_parse.c
)

target_link_libraries(unit_tests PRIVATE trino_odbc CURL::libcurl Threads::Threads)
target_include_directories(unit_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_test(NAME unit_tests COMMAND unit_tests)

# Integration tests (require running Trino instance)
if(TRINO_INTEGRATION_TEST_URL)
    add_executable(integration_tests
        test/integration/test_connect.c
        test/integration/test_query.c
        test/integration/test_resultset.c
    )

    target_link_libraries(integration_tests PRIVATE trino_odbc CURL::libcurl Threads::Threads)
    target_include_directories(integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_compile_definitions(integration_tests PRIVATE
        TRINO_TEST_URL="${TRINO_INTEGRATION_TEST_URL}"
    )

    add_test(NAME integration_tests COMMAND integration_tests)
    set_tests_properties(integration_tests PROPERTIES LABELS "integration")
endif()
