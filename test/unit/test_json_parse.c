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

#define ASSERT_STREQ(a, b) do { tests_run++; if (strcmp((a),(b)) != 0) { printf("FAIL: %s:%d \"%s\" != \"%s\"\n", __func__, __LINE__, (a), (b)); return; } tests_passed++; } while(0)
#define ASSERT_TRUE(c) do { tests_run++; if (!(c)) { printf("FAIL: %s:%d\n", __func__, __LINE__); return; } tests_passed++; } while(0)
#define ASSERT_NULL(p) do { tests_run++; if ((p)) { printf("FAIL: %s:%d not NULL\n", __func__, __LINE__); return; } tests_passed++; } while(0)

/* Full QueryResults: columns + data + stats parsed into the results struct. */
TEST(parse_query_response_rows)
{
    const char *json =
        "{\"id\":\"q-1\","
        "\"columns\":[{\"name\":\"id\",\"type\":\"integer\"},"
        "{\"name\":\"name\",\"type\":\"varchar\"}],"
        "\"data\":[[1,\"alice\"],[2,\"bob\"],[3,null]],"
        "\"stats\":{\"state\":\"FINISHED\",\"processedRows\":3}}";

    trino_query_results_t *r = calloc(1, sizeof(*r));
    ASSERT_NOT_NULL(r);

    SQLRETURN ret = trino_parse_query_response(json, r);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_EQ(r->has_error, false);
    ASSERT_EQ(r->column_count, 2);
    ASSERT_EQ(r->row_count, 3);
    ASSERT_EQ(r->state, TRINO_QUERY_STATE_FINISHED);
    ASSERT_STREQ((char *)r->query_id, "q-1");

    /* Cell values are stored as strings; NULL cells are NULL pointers. */
    ASSERT_NOT_NULL(r->rows);
    ASSERT_STREQ((char *)r->rows[0][0], "1");
    ASSERT_STREQ((char *)r->rows[0][1], "alice");
    ASSERT_STREQ((char *)r->rows[1][0], "2");
    ASSERT_STREQ((char *)r->rows[1][1], "bob");
    ASSERT_STREQ((char *)r->rows[2][0], "3");
    ASSERT_NULL(r->rows[2][1]); /* SQL NULL */

    trino_query_results_free(r);
}

/* Two pages accumulate into a single row set; columns parsed only once. */
TEST(parse_query_response_pagination)
{
    trino_query_results_t *r = calloc(1, sizeof(*r));
    ASSERT_NOT_NULL(r);

    const char *page1 =
        "{\"id\":\"q-2\","
        "\"columns\":[{\"name\":\"n\",\"type\":\"bigint\"}],"
        "\"data\":[[10],[20]],"
        "\"nextUri\":\"http://h/next\","
        "\"stats\":{\"state\":\"RUNNING\"}}";
    const char *page2 =
        "{\"id\":\"q-2\","
        "\"data\":[[30],[40],[50]],"
        "\"stats\":{\"state\":\"FINISHED\"}}";

    ASSERT_EQ(trino_parse_query_response(page1, r), SQL_SUCCESS);
    ASSERT_EQ(r->column_count, 1);
    ASSERT_EQ(r->row_count, 2);
    ASSERT_NOT_NULL(r->next_uri);

    ASSERT_EQ(trino_parse_query_response(page2, r), SQL_SUCCESS);
    ASSERT_EQ(r->column_count, 1);   /* unchanged */
    ASSERT_EQ(r->row_count, 5);      /* accumulated */
    ASSERT_NULL(r->next_uri);        /* cleared on final page */
    ASSERT_EQ(r->state, TRINO_QUERY_STATE_FINISHED);

    ASSERT_STREQ((char *)r->rows[0][0], "10");
    ASSERT_STREQ((char *)r->rows[4][0], "50");

    trino_query_results_free(r);
}

/* An error response populates error fields and returns SQL_ERROR. */
TEST(parse_query_response_error)
{
    const char *json =
        "{\"id\":\"q-3\","
        "\"error\":{\"message\":\"line 1:8: Column 'x' cannot be resolved\","
        "\"errorName\":\"COLUMN_NOT_FOUND\",\"errorType\":\"USER_ERROR\"}}";

    trino_query_results_t *r = calloc(1, sizeof(*r));
    ASSERT_NOT_NULL(r);

    SQLRETURN ret = trino_parse_query_response(json, r);
    ASSERT_EQ(ret, SQL_ERROR);
    ASSERT_EQ(r->has_error, true);
    ASSERT_NOT_NULL(r->error_message);
    ASSERT_NOT_NULL(r->error_name);
    ASSERT_STREQ((char *)r->error_name, "COLUMN_NOT_FOUND");
    ASSERT_EQ(r->state, TRINO_QUERY_STATE_FAILED);

    trino_query_results_free(r);
}

/* Complex cell types (array/map/row) surface as serialized JSON strings. */
TEST(parse_query_response_complex_cells)
{
    const char *json =
        "{\"id\":\"q-4\","
        "\"columns\":[{\"name\":\"tags\",\"type\":\"array(varchar)\"},"
        "{\"name\":\"props\",\"type\":\"map(varchar,integer)\"}],"
        "\"data\":[[[\"a\",\"b\"],{\"k\":1}]],"
        "\"stats\":{\"state\":\"FINISHED\"}}";

    trino_query_results_t *r = calloc(1, sizeof(*r));
    ASSERT_NOT_NULL(r);

    ASSERT_EQ(trino_parse_query_response(json, r), SQL_SUCCESS);
    ASSERT_EQ(r->column_count, 2);
    ASSERT_EQ(r->row_count, 1);
    /* Complex types map to VARCHAR and serialize back to JSON text. */
    ASSERT_EQ(r->columns[0].odbc_type, SQL_VARCHAR);
    ASSERT_STREQ((char *)r->rows[0][0], "[\"a\",\"b\"]");
    ASSERT_STREQ((char *)r->rows[0][1], "{\"k\":1}");

    trino_query_results_free(r);
}

/* HTTP header value sanitization strips CR/LF and truncates safely. */
TEST(http_header_sanitize)
{
    char out[64];

    /* Normal value passes through unchanged. */
    trino_http_sanitize_header_value("trino-odbc", out, sizeof(out));
    ASSERT_EQ(strcmp(out, "trino-odbc"), 0);

    /* CRLF injection attempt: control chars are stripped, leaving the bytes
     * concatenated (no second header can be injected). */
    trino_http_sanitize_header_value("evil\r\nX-Inject: 1", out, sizeof(out));
    ASSERT_EQ(strcmp(out, "evilX-Inject: 1"), 0);

    /* Bare CR and LF are both removed. */
    trino_http_sanitize_header_value("a\rb\nc", out, sizeof(out));
    ASSERT_EQ(strcmp(out, "abc"), 0);

    /* Truncation respects the output buffer size. */
    char small[5];
    trino_http_sanitize_header_value("abcdefgh", small, sizeof(small));
    ASSERT_EQ(strcmp(small, "abcd"), 0);

    /* NULL input yields an empty string. */
    trino_http_sanitize_header_value(NULL, out, sizeof(out));
    ASSERT_EQ(strcmp(out, ""), 0);
}

/* main() is in test_connection_string.c — it calls all test functions */
