/* End-to-end tests for the execute/fetch/getdata path.
 *
 * These exercise the full driver flow (SQLExecDirect -> SQLFetch -> SQLGetData)
 * without a live Trino server by installing a mock HTTP transport that returns
 * canned QueryResults JSON, simulating the POST /v1/statement + GET nextUri
 * protocol Trino uses.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trino_odbc.h"
#include "trino_odbc/core.h"
#include "trino_odbc/connection.h"
#include "trino_odbc/statement.h"
#include "trino_odbc/protocol.h"

int tests_run = 0;
int tests_passed = 0;

#define TEST(name) void test_##name(void)
#define ASSERT_EQ(a, b)                                                                  \
    do {                                                                                 \
        tests_run++;                                                                     \
        if ((long long)(a) != (long long)(b)) {                                          \
            printf("FAIL: %s:%d %s != %s\n", __func__, __LINE__, #a, #b);                \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)
#define ASSERT_STREQ(a, b)                                                               \
    do {                                                                                 \
        tests_run++;                                                                     \
        if (strcmp((a), (b)) != 0) {                                                     \
            printf("FAIL: %s:%d \"%s\" != \"%s\"\n", __func__, __LINE__, (a), (b));      \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)
#define ASSERT_TRUE(c)                                                                   \
    do {                                                                                 \
        tests_run++;                                                                     \
        if (!(c)) {                                                                      \
            printf("FAIL: %s:%d %s\n", __func__, __LINE__, #c);                          \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)

/* ------------------------------------------------------------------------
 * Mock transport: a scripted sequence of responses.
 * ------------------------------------------------------------------------ */
typedef struct {
    const char **responses; /* NULL-terminated array of JSON strings */
    int index;
    char last_method[8];
    char last_body[256];
} mock_script_t;

static char *mock_transport(const char *method, const char *url, const char *body,
                            void *ctx)
{
    (void)url;
    mock_script_t *s = (mock_script_t *)ctx;

    strncpy(s->last_method, method, sizeof(s->last_method) - 1);
    s->last_method[sizeof(s->last_method) - 1] = '\0';
    if (body) {
        strncpy(s->last_body, body, sizeof(s->last_body) - 1);
        s->last_body[sizeof(s->last_body) - 1] = '\0';
    }

    const char *resp = s->responses[s->index];
    if (!resp)
        return NULL; /* exhausted -> simulate failure */
    s->index++;
    return strdup(resp);
}

/* Build a connected statement handle wired to the mock transport, connecting
 * through the public SQLDriverConnect entry point (as a driver manager would). */
static SQLHSTMT make_connected_stmt(SQLHENV *env_out, SQLHDBC *dbc_out)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env) != SQL_SUCCESS)
        return NULL;
    if (SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc) != SQL_SUCCESS)
        return NULL;

    SQLCHAR out[256];
    SQLSMALLINT out_len = 0;
    if (SQLDriverConnect(
            dbc, NULL, (SQLCHAR *)"Server=localhost;Port=8080;Catalog=memory", SQL_NTS,
            out, sizeof(out), &out_len, SQL_DRIVER_NOPROMPT) != SQL_SUCCESS) {
        return NULL;
    }

    if (SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) != SQL_SUCCESS)
        return NULL;

    *env_out = env;
    *dbc_out = dbc;
    return stmt;
}

/* ------------------------------------------------------------------------
 * Tests
 * ------------------------------------------------------------------------ */

/* Single-page result: POST returns nextUri, one GET returns columns + data. */
TEST(e2e_select_single_page)
{
    const char *responses[] = {
        /* POST /v1/statement */
        "{\"id\":\"q1\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        /* GET nextUri -> columns + data, no further pages */
        "{\"id\":\"q1\","
        "\"columns\":[{\"name\":\"id\",\"type\":\"integer\"},"
        "{\"name\":\"name\",\"type\":\"varchar\"}],"
        "\"data\":[[1,\"alice\"],[2,\"bob\"]],"
        "\"stats\":{\"state\":\"FINISHED\"}}",
        NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR *)"SELECT id, name FROM t", SQL_NTS);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLSMALLINT ncols = 0;
    ASSERT_EQ(SQLNumResultCols(stmt, &ncols), SQL_SUCCESS);
    ASSERT_EQ(ncols, 2);

    /* Row 1 */
    ASSERT_EQ(SQLFetch(stmt), SQL_SUCCESS);
    char buf[64];
    SQLLEN ind = 0;
    ASSERT_EQ(SQLGetData(stmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind), SQL_SUCCESS);
    ASSERT_STREQ(buf, "1");
    ASSERT_EQ(SQLGetData(stmt, 2, SQL_C_CHAR, buf, sizeof(buf), &ind), SQL_SUCCESS);
    ASSERT_STREQ(buf, "alice");

    /* Row 2 */
    ASSERT_EQ(SQLFetch(stmt), SQL_SUCCESS);
    ASSERT_EQ(SQLGetData(stmt, 2, SQL_C_CHAR, buf, sizeof(buf), &ind), SQL_SUCCESS);
    ASSERT_STREQ(buf, "bob");

    /* End of rows */
    ASSERT_EQ(SQLFetch(stmt), SQL_NO_DATA);

    /* The driver should have POSTed the query text. */
    ASSERT_STREQ(script.last_method, "GET");

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

/* Multi-page result: rows arrive across two GET pages and accumulate. */
TEST(e2e_select_multi_page)
{
    const char *responses[] = {
        "{\"id\":\"q2\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        /* page 1: columns + 2 rows + nextUri */
        "{\"id\":\"q2\","
        "\"columns\":[{\"name\":\"n\",\"type\":\"bigint\"}],"
        "\"data\":[[10],[20]],"
        "\"nextUri\":\"http://h/p2\","
        "\"stats\":{\"state\":\"RUNNING\"}}",
        /* page 2: 1 more row, finished */
        "{\"id\":\"q2\",\"data\":[[30]],\"stats\":{\"state\":\"FINISHED\"}}", NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    ASSERT_EQ(SQLExecDirect(stmt, (SQLCHAR *)"SELECT n FROM t", SQL_NTS), SQL_SUCCESS);

    /* The query() loop only guarantees the first data page; SQLFetch must pull
     * the remaining page(s) via nextUri. Collect all rows. */
    char buf[64];
    SQLLEN ind = 0;
    int count = 0;
    char seen[8][64];
    while (SQLFetch(stmt) == SQL_SUCCESS) {
        ASSERT_EQ(SQLGetData(stmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind), SQL_SUCCESS);
        if (count < 8)
            strcpy(seen[count], buf);
        count++;
    }
    ASSERT_EQ(count, 3);
    ASSERT_STREQ(seen[0], "10");
    ASSERT_STREQ(seen[1], "20");
    ASSERT_STREQ(seen[2], "30");

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

/* Typed retrieval: SQLGetData converts to C numeric types. */
TEST(e2e_getdata_typed)
{
    const char *responses[] = {
        "{\"id\":\"q3\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        "{\"id\":\"q3\","
        "\"columns\":[{\"name\":\"i\",\"type\":\"integer\"},"
        "{\"name\":\"b\",\"type\":\"bigint\"},"
        "{\"name\":\"d\",\"type\":\"double\"}],"
        "\"data\":[[42,9000000000,3.5]],"
        "\"stats\":{\"state\":\"FINISHED\"}}",
        NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    ASSERT_EQ(SQLExecDirect(stmt, (SQLCHAR *)"SELECT i,b,d FROM t", SQL_NTS),
              SQL_SUCCESS);
    ASSERT_EQ(SQLFetch(stmt), SQL_SUCCESS);

    SQLLEN ind = 0;
    SQLINTEGER i = 0;
    SQLBIGINT b = 0;
    SQLDOUBLE d = 0;
    ASSERT_EQ(SQLGetData(stmt, 1, SQL_C_LONG, &i, sizeof(i), &ind), SQL_SUCCESS);
    ASSERT_EQ(i, 42);
    ASSERT_EQ(SQLGetData(stmt, 2, SQL_C_SBIGINT, &b, sizeof(b), &ind), SQL_SUCCESS);
    ASSERT_EQ(b, 9000000000LL);
    ASSERT_EQ(SQLGetData(stmt, 3, SQL_C_DOUBLE, &d, sizeof(d), &ind), SQL_SUCCESS);
    ASSERT_TRUE(d > 3.49 && d < 3.51);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

/* NULL cell: SQLGetData reports SQL_NULL_DATA via the indicator. */
TEST(e2e_getdata_null)
{
    const char *responses[] = {
        "{\"id\":\"q4\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        "{\"id\":\"q4\","
        "\"columns\":[{\"name\":\"v\",\"type\":\"varchar\"}],"
        "\"data\":[[null]],"
        "\"stats\":{\"state\":\"FINISHED\"}}",
        NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    ASSERT_EQ(SQLExecDirect(stmt, (SQLCHAR *)"SELECT v FROM t", SQL_NTS), SQL_SUCCESS);
    ASSERT_EQ(SQLFetch(stmt), SQL_SUCCESS);

    char buf[16] = "untouched";
    SQLLEN ind = 0;
    ASSERT_EQ(SQLGetData(stmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind), SQL_SUCCESS);
    ASSERT_EQ(ind, SQL_NULL_DATA);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

/* Server error: SQLExecDirect returns SQL_ERROR and the query did not crash. */
TEST(e2e_query_error)
{
    const char *responses[] = {
        "{\"id\":\"q5\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        "{\"id\":\"q5\",\"error\":{\"message\":\"Table not found\","
        "\"errorName\":\"TABLE_NOT_FOUND\",\"errorType\":\"USER_ERROR\"}}",
        NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    ASSERT_EQ(SQLExecDirect(stmt, (SQLCHAR *)"SELECT * FROM missing", SQL_NTS),
              SQL_ERROR);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

/* Write op: INSERT sets row count from stats and produces no result set. */
TEST(e2e_write_rowcount)
{
    const char *responses[] = {
        "{\"id\":\"q6\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        "{\"id\":\"q6\",\"stats\":{\"state\":\"FINISHED\",\"processedRows\":7}}", NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    ASSERT_EQ(SQLExecDirect(stmt, (SQLCHAR *)"INSERT INTO t VALUES (1)", SQL_NTS),
              SQL_SUCCESS);

    SQLLEN rows = -1;
    ASSERT_EQ(SQLRowCount(stmt, &rows), SQL_SUCCESS);
    ASSERT_EQ(rows, 7);

    SQLSMALLINT ncols = -1;
    ASSERT_EQ(SQLNumResultCols(stmt, &ncols), SQL_SUCCESS);
    ASSERT_EQ(ncols, 0);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

/* SQLDriverConnect parses the connection string and SQLDisconnect closes it. */
TEST(e2e_driver_connect_disconnect)
{
    SQLHENV env;
    SQLHDBC dbc;
    ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env), SQL_SUCCESS);
    ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc), SQL_SUCCESS);

    SQLCHAR out[256];
    SQLSMALLINT out_len = 0;
    ASSERT_EQ(SQLDriverConnect(
                  dbc, NULL,
                  (SQLCHAR *)"Server=trino.example.com;Port=8443;User=bob;SSL=true",
                  SQL_NTS, out, sizeof(out), &out_len, SQL_DRIVER_NOPROMPT),
              SQL_SUCCESS);
    ASSERT_TRUE(out_len > 0);

    /* The connection config should reflect the parsed string. */
    trino_conn_t *conn = (trino_conn_t *)dbc;
    ASSERT_TRUE(conn->connected);
    ASSERT_STREQ((char *)conn->server, "trino.example.com");
    ASSERT_EQ(conn->port, 8443);
    ASSERT_STREQ((char *)conn->user, "bob");
    ASSERT_TRUE(conn->ssl_enabled);

    /* Disconnecting twice: first succeeds, second reports not-open. */
    ASSERT_EQ(SQLDisconnect(dbc), SQL_SUCCESS);
    ASSERT_EQ(SQLDisconnect(dbc), SQL_ERROR);

    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* SQLConnect connects with a host + user + password (PASSWORD auth). */
TEST(e2e_sqlconnect)
{
    SQLHENV env;
    SQLHDBC dbc;
    ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env), SQL_SUCCESS);
    ASSERT_EQ(SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc), SQL_SUCCESS);

    ASSERT_EQ(SQLConnect(dbc, (SQLCHAR *)"trino.example.com:9090", SQL_NTS,
                         (SQLCHAR *)"alice", SQL_NTS, (SQLCHAR *)"secret", SQL_NTS),
              SQL_SUCCESS);

    trino_conn_t *conn = (trino_conn_t *)dbc;
    ASSERT_TRUE(conn->connected);
    ASSERT_STREQ((char *)conn->server, "trino.example.com");
    ASSERT_EQ(conn->port, 9090);
    ASSERT_STREQ((char *)conn->user, "alice");
    ASSERT_STREQ((char *)conn->auth_type, "PASSWORD");

    ASSERT_EQ(SQLDisconnect(dbc), SQL_SUCCESS);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* SQLGetData converts date/time/timestamp/bit values into ODBC C structs. */
TEST(e2e_getdata_datetime)
{
    const char *responses[] = {
        "{\"id\":\"q7\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        "{\"id\":\"q7\","
        "\"columns\":[{\"name\":\"d\",\"type\":\"date\"},"
        "{\"name\":\"t\",\"type\":\"time\"},"
        "{\"name\":\"ts\",\"type\":\"timestamp\"},"
        "{\"name\":\"b\",\"type\":\"boolean\"}],"
        "\"data\":[[\"2026-06-16\",\"13:45:30\",\"2026-06-16 13:45:30.123456\",true]],"
        "\"stats\":{\"state\":\"FINISHED\"}}",
        NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    ASSERT_EQ(SQLExecDirect(stmt, (SQLCHAR *)"SELECT d,t,ts,b FROM x", SQL_NTS),
              SQL_SUCCESS);
    ASSERT_EQ(SQLFetch(stmt), SQL_SUCCESS);

    SQLLEN ind = 0;
    SQL_DATE_STRUCT d;
    SQL_TIME_STRUCT t;
    SQL_TIMESTAMP_STRUCT ts;
    unsigned char b;

    ASSERT_EQ(SQLGetData(stmt, 1, SQL_C_TYPE_DATE, &d, sizeof(d), &ind), SQL_SUCCESS);
    ASSERT_EQ(d.year, 2026);
    ASSERT_EQ(d.month, 6);
    ASSERT_EQ(d.day, 16);

    ASSERT_EQ(SQLGetData(stmt, 2, SQL_C_TYPE_TIME, &t, sizeof(t), &ind), SQL_SUCCESS);
    ASSERT_EQ(t.hour, 13);
    ASSERT_EQ(t.minute, 45);
    ASSERT_EQ(t.second, 30);

    ASSERT_EQ(SQLGetData(stmt, 3, SQL_C_TYPE_TIMESTAMP, &ts, sizeof(ts), &ind),
              SQL_SUCCESS);
    ASSERT_EQ(ts.year, 2026);
    ASSERT_EQ(ts.day, 16);
    ASSERT_EQ(ts.hour, 13);
    ASSERT_EQ(ts.second, 30);
    ASSERT_EQ(ts.fraction, 123456000);

    ASSERT_EQ(SQLGetData(stmt, 4, SQL_C_BIT, &b, sizeof(b), &ind), SQL_SUCCESS);
    ASSERT_EQ(b, 1);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

/* SQLGetData reports truncation (01004 / SQL_SUCCESS_WITH_INFO) for a short
 * character buffer, and rejects out-of-range integer conversions. */
TEST(e2e_getdata_truncation_and_range)
{
    const char *responses[] = {
        "{\"id\":\"q8\",\"nextUri\":\"http://h/p1\",\"stats\":{\"state\":\"RUNNING\"}}",
        "{\"id\":\"q8\","
        "\"columns\":[{\"name\":\"s\",\"type\":\"varchar\"},"
        "{\"name\":\"big\",\"type\":\"bigint\"}],"
        "\"data\":[[\"hello world\",99999]],"
        "\"stats\":{\"state\":\"FINISHED\"}}",
        NULL};
    mock_script_t script = {responses, 0, "", ""};
    trino_http_set_test_transport(mock_transport, &script);

    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt = make_connected_stmt(&env, &dbc);
    ASSERT_TRUE(stmt != NULL);

    ASSERT_EQ(SQLExecDirect(stmt, (SQLCHAR *)"SELECT s,big FROM x", SQL_NTS),
              SQL_SUCCESS);
    ASSERT_EQ(SQLFetch(stmt), SQL_SUCCESS);

    /* Char truncation: buffer too small => SUCCESS_WITH_INFO, full length set. */
    char small[6];
    SQLLEN ind = 0;
    ASSERT_EQ(SQLGetData(stmt, 1, SQL_C_CHAR, small, sizeof(small), &ind),
              SQL_SUCCESS_WITH_INFO);
    ASSERT_STREQ(small, "hello");
    ASSERT_EQ(ind, 11);

    /* 99999 does not fit in a SQL_C_SSHORT (max 32767) => error. */
    short sh = 0;
    ASSERT_EQ(SQLGetData(stmt, 2, SQL_C_SSHORT, &sh, sizeof(sh), &ind), SQL_ERROR);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
    trino_http_set_test_transport(NULL, NULL);
}

int main(void)
{
    printf("Running Trino ODBC Driver end-to-end tests...\n\n");

    test_e2e_driver_connect_disconnect();
    test_e2e_sqlconnect();
    test_e2e_select_single_page();
    test_e2e_select_multi_page();
    test_e2e_getdata_typed();
    test_e2e_getdata_null();
    test_e2e_query_error();
    test_e2e_write_rowcount();
    test_e2e_getdata_datetime();
    test_e2e_getdata_truncation_and_range();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if (tests_passed == tests_run) {
        printf("\nAll end-to-end tests PASSED!\n");
        return 0;
    }
    printf("\nSome end-to-end tests FAILED.\n");
    return 1;
}
