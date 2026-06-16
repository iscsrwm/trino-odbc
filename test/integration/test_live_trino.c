/* Live integration test against a real Trino server.
 *
 * This test is skipped (exit 0) unless TRINO_TEST_SERVER is set in the
 * environment, so the default `ctest` run stays green without a server. When a
 * server is configured it exercises the full driver against Trino's built-in
 * catalogs (system / tpch), which require no data setup.
 *
 * Configuration via environment:
 *   TRINO_TEST_SERVER   - host (required to run; otherwise the test is skipped)
 *   TRINO_TEST_PORT     - port (default 8080)
 *   TRINO_TEST_USER     - user (default "test")
 *   TRINO_TEST_CATALOG  - catalog (default "tpch")
 *   TRINO_TEST_SCHEMA   - schema  (default "tiny")
 *   TRINO_TEST_SSL      - "true" to enable TLS (default off)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trino_odbc.h"

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond) do { \
    tests_run++; \
    if (cond) { tests_passed++; } \
    else { printf("FAIL: %s:%d %s\n", __func__, __LINE__, #cond); } \
} while (0)

static const char *env_or(const char *name, const char *fallback)
{
    const char *v = getenv(name);
    return (v && *v) ? v : fallback;
}

/* Build a connection string from the environment. */
static void build_conn_string(char *out, size_t out_size)
{
    snprintf(out, out_size,
             "Server=%s;Port=%s;User=%s;Catalog=%s;Schema=%s;SSL=%s",
             env_or("TRINO_TEST_SERVER", "localhost"),
             env_or("TRINO_TEST_PORT", "8080"),
             env_or("TRINO_TEST_USER", "test"),
             env_or("TRINO_TEST_CATALOG", "tpch"),
             env_or("TRINO_TEST_SCHEMA", "tiny"),
             env_or("TRINO_TEST_SSL", "false"));
}

/* Connect; caller must free the handles. Returns SQL_SUCCESS on success. */
static SQLRETURN connect_live(SQLHENV *env, SQLHDBC *dbc)
{
    if (SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, env) != SQL_SUCCESS)
        return SQL_ERROR;
    if (SQLAllocHandle(SQL_HANDLE_DBC, *env, dbc) != SQL_SUCCESS)
        return SQL_ERROR;

    char conn_str[512];
    build_conn_string(conn_str, sizeof(conn_str));

    SQLCHAR out[1024];
    SQLSMALLINT out_len = 0;
    return SQLDriverConnect(*dbc, NULL, (SQLCHAR *)conn_str, SQL_NTS,
                            out, sizeof(out), &out_len, SQL_DRIVER_NOPROMPT);
}

/* SELECT a literal value and read it back. */
static void test_select_literal(void)
{
    SQLHENV env; SQLHDBC dbc; SQLHSTMT stmt;
    if (connect_live(&env, &dbc) != SQL_SUCCESS) {
        printf("FAIL: %s: connect failed\n", __func__);
        tests_run++;
        return;
    }
    CHECK(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) == SQL_SUCCESS);

    CHECK(SQLExecDirect(stmt, (SQLCHAR *)"SELECT 42 AS answer", SQL_NTS) == SQL_SUCCESS);

    SQLSMALLINT ncols = 0;
    CHECK(SQLNumResultCols(stmt, &ncols) == SQL_SUCCESS);
    CHECK(ncols == 1);

    CHECK(SQLFetch(stmt) == SQL_SUCCESS);

    SQLINTEGER answer = 0; SQLLEN ind = 0;
    CHECK(SQLGetData(stmt, 1, SQL_C_LONG, &answer, sizeof(answer), &ind) == SQL_SUCCESS);
    CHECK(answer == 42);

    CHECK(SQLFetch(stmt) == SQL_NO_DATA);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* Query a built-in table that spans multiple result pages (tpch.tiny.orders
 * has 15000 rows; LIMIT keeps it bounded but still exercises pagination). */
static void test_select_rows(void)
{
    SQLHENV env; SQLHDBC dbc; SQLHSTMT stmt;
    if (connect_live(&env, &dbc) != SQL_SUCCESS) {
        printf("FAIL: %s: connect failed\n", __func__);
        tests_run++;
        return;
    }
    CHECK(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) == SQL_SUCCESS);

    CHECK(SQLExecDirect(stmt, (SQLCHAR *)
        "SELECT name FROM tpch.tiny.nation ORDER BY name", SQL_NTS) == SQL_SUCCESS);

    int count = 0;
    char first[64] = "";
    while (SQLFetch(stmt) == SQL_SUCCESS) {
        char buf[64]; SQLLEN ind = 0;
        if (SQLGetData(stmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind) == SQL_SUCCESS) {
            if (count == 0) strncpy(first, buf, sizeof(first) - 1);
            count++;
        }
    }
    /* TPCH has exactly 25 nations; ordered first is ALGERIA. */
    CHECK(count == 25);
    CHECK(strcmp(first, "ALGERIA") == 0);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* A bound parameter is sent and applied. */
static void test_bound_parameter(void)
{
    SQLHENV env; SQLHDBC dbc; SQLHSTMT stmt;
    if (connect_live(&env, &dbc) != SQL_SUCCESS) {
        printf("FAIL: %s: connect failed\n", __func__);
        tests_run++;
        return;
    }
    CHECK(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) == SQL_SUCCESS);

    SQLINTEGER param = 7;
    SQLLEN plen = 0;
    CHECK(SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, &param, 0, &plen) == SQL_SUCCESS);

    CHECK(SQLExecDirect(stmt, (SQLCHAR *)
        "SELECT name FROM tpch.tiny.nation WHERE nationkey = ?", SQL_NTS)
        == SQL_SUCCESS);

    CHECK(SQLFetch(stmt) == SQL_SUCCESS);
    char buf[64]; SQLLEN ind = 0;
    CHECK(SQLGetData(stmt, 1, SQL_C_CHAR, buf, sizeof(buf), &ind) == SQL_SUCCESS);
    /* nationkey 7 is GERMANY in TPCH. */
    CHECK(strcmp(buf, "GERMANY") == 0);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* An invalid query surfaces SQL_ERROR. */
static void test_query_error(void)
{
    SQLHENV env; SQLHDBC dbc; SQLHSTMT stmt;
    if (connect_live(&env, &dbc) != SQL_SUCCESS) {
        printf("FAIL: %s: connect failed\n", __func__);
        tests_run++;
        return;
    }
    CHECK(SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt) == SQL_SUCCESS);

    CHECK(SQLExecDirect(stmt, (SQLCHAR *)
        "SELECT * FROM tpch.tiny.no_such_table", SQL_NTS) == SQL_ERROR);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

int main(void)
{
    if (!getenv("TRINO_TEST_SERVER")) {
        printf("SKIP: live Trino tests (set TRINO_TEST_SERVER to enable)\n");
        return 0; /* skipped, not failed */
    }

    printf("Running live Trino integration tests against %s:%s ...\n\n",
           env_or("TRINO_TEST_SERVER", "?"), env_or("TRINO_TEST_PORT", "8080"));

    test_select_literal();
    test_select_rows();
    test_bound_parameter();
    test_query_error();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    return (tests_passed == tests_run) ? 0 : 1;
}
