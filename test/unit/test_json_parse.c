#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trino_odbc.h"
#include "trino_odbc/protocol.h"

/* Shared counters defined in test_handle.c */
extern int tests_run;
extern int tests_passed;

#define TEST(name) void test_##name(void)
#define ASSERT_EQ(a, b) do { tests_run++; if ((a) != (b)) { printf("FAIL: %s:%d\n", __func__, __LINE__); return; } tests_passed++; } while(0)
#define ASSERT_NOT_NULL(p) do { tests_run++; if (!(p)) { printf("FAIL: %s:%d NULL\n", __func__, __LINE__); return; } tests_passed++; } while(0)

TEST(json_parse_columns)
{
    const char *json =
        "{\"columns\":["
        "{\"name\":\"id\",\"type\":\"integer\"},"
        "{\"name\":\"name\",\"type\":\"varchar\"},"
        "{\"name\":\"created\",\"type\":\"timestamp\"}"
        "]}";

    SQLULEN col_count = 0;
    trino_column_meta_t *cols = trino_parse_columns(json, &col_count);

    ASSERT_EQ(col_count, 3);
    ASSERT_NOT_NULL(cols);

    if (cols) {
        ASSERT_EQ(strcmp((char *)cols[0].name, "id"), 0);
        ASSERT_EQ(cols[0].odbc_type, SQL_INTEGER);
        ASSERT_EQ(strcmp((char *)cols[1].name, "name"), 0);
        ASSERT_EQ(cols[1].odbc_type, SQL_VARCHAR);
        ASSERT_EQ(strcmp((char *)cols[2].name, "created"), 0);
        ASSERT_EQ(cols[2].odbc_type, SQL_TYPE_TIMESTAMP);

        free(cols);
    }
}

TEST(json_parse_empty_columns)
{
    const char *json = "{\"columns\":[]}";

    SQLULEN col_count = 0;
    trino_column_meta_t *cols = trino_parse_columns(json, &col_count);

    ASSERT_EQ(col_count, 0);
    /* Empty columns may return NULL or empty array */
    if (cols) free(cols);
}

TEST(json_parse_no_columns)
{
    const char *json = "{\"id\":\"query-123\"}";

    SQLULEN col_count = 0;
    trino_column_meta_t *cols = trino_parse_columns(json, &col_count);

    ASSERT_EQ(col_count, 0);
    if (cols) free(cols);
}

TEST(query_results_free)
{
    trino_query_results_t *results = calloc(1, sizeof(*results));
    results->next_uri = strdup("http://test/next");
    results->error_name = strdup("TEST_ERROR");
    results->error_message = strdup("Test error message");

    trino_query_results_free(results);
    /* Should not crash — just verify no double-free */
    tests_run++;
    tests_passed++;
}

/* main() is in test_connection_string.c — it calls all test functions */
