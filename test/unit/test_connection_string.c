#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trino_odbc.h"
#include "trino_odbc/connection.h"

/* Test functions declared in other test files */
extern void test_diag_init(void);
extern void test_diag_add_record(void);
extern void test_diag_clear(void);
extern void test_handle_alloc_env(void);
extern void test_handle_alloc_conn(void);
extern void test_handle_alloc_stmt(void);
extern void test_handle_free(void);
extern void test_handle_invalid(void);
extern void test_env_attributes(void);
extern void test_type_mapping(void);
extern void test_json_parse_columns(void);
extern void test_json_parse_empty_columns(void);
extern void test_json_parse_no_columns(void);
extern void test_query_results_free(void);
extern void test_trino_to_odbc_types(void);
extern void test_type_name_lookup(void);

/* Shared test counters */
extern int tests_run;
extern int tests_passed;

#define TEST(name) void test_##name(void)
#define ASSERT_EQ(a, b) do { tests_run++; if ((a) != (b)) { printf("FAIL: %s:%d %s != %s\n", __func__, __LINE__, #a, #b); return; } tests_passed++; } while(0)
#define ASSERT_STREQ(a, b) do { tests_run++; if (strcmp((a),(b)) != 0) { printf("FAIL: %s:%d %s != %s\n", __func__, __LINE__, #a, #b); return; } tests_passed++; } while(0)

TEST(conn_string_basic)
{
    trino_conn_config_t config;
    SQLRETURN ret = trino_parse_conn_string(
        "Server=myhost;Port=8443;User=admin;Catalog=hive;Schema=default",
        &config);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_STREQ((char *)config.server, "myhost");
    ASSERT_EQ(config.port, 8443);
    ASSERT_STREQ((char *)config.user, "admin");
    ASSERT_STREQ((char *)config.catalog, "hive");
    ASSERT_STREQ((char *)config.schema, "default");
}

TEST(conn_string_defaults)
{
    trino_conn_config_t config;
    SQLRETURN ret = trino_parse_conn_string("", &config);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_STREQ((char *)config.server, "localhost");
    ASSERT_EQ(config.port, 8080);
    ASSERT_STREQ((char *)config.auth_type, "NONE");
}

TEST(conn_string_ssl)
{
    trino_conn_config_t config;
    SQLRETURN ret = trino_parse_conn_string(
        "Server=secure;SSL=true;Authentication=PASSWORD",
        &config);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_EQ(config.ssl_enabled, 1);
    ASSERT_STREQ((char *)config.auth_type, "PASSWORD");
}

TEST(conn_config_defaults)
{
    trino_conn_config_t config;
    trino_conn_config_defaults(&config);
    ASSERT_STREQ((char *)config.server, "localhost");
    ASSERT_EQ(config.port, 8080);
    ASSERT_STREQ((char *)config.auth_type, "NONE");
    ASSERT_EQ(config.ssl_enabled, 0);
}

/* This is the single main for all tests */
int main(void)
{
    printf("Running Trino ODBC Driver unit tests...\n\n");

    /* Handle tests */
    test_diag_init();
    test_diag_add_record();
    test_diag_clear();
    test_handle_alloc_env();
    test_handle_alloc_conn();
    test_handle_alloc_stmt();
    test_handle_free();
    test_handle_invalid();
    test_env_attributes();

    /* Connection string tests */
    test_conn_string_basic();
    test_conn_string_defaults();
    test_conn_string_ssl();
    test_conn_config_defaults();

    /* Type conversion tests */
    test_type_mapping();
    test_trino_to_odbc_types();
    test_type_name_lookup();

    /* JSON parse tests */
    test_json_parse_columns();
    test_json_parse_empty_columns();
    test_json_parse_no_columns();
    test_query_results_free();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if (tests_passed == tests_run) {
        printf("\nAll tests PASSED!\n");
        return 0;
    } else {
        printf("\nSome tests FAILED.\n");
        return 1;
    }
}
