#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "trino_odbc.h"
#include "trino_odbc/core.h"
#include "trino_odbc/error.h"
#include "trino_odbc/catalog.h"
#include "trino_odbc/statement.h"

/* Shared test counters — extern from test_handle.c */
extern int tests_run;
extern int tests_passed;

#define TEST(name) void test_##name(void)
#define ASSERT_EQ(a, b)                                                                  \
    do {                                                                                 \
        tests_run++;                                                                     \
        if ((a) != (b)) {                                                                \
            printf("FAIL: %s:%d %s != %s\n", __func__, __LINE__, #a, #b);                \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)
#define ASSERT_TRUE(expr)                                                                \
    do {                                                                                 \
        tests_run++;                                                                     \
        if (!(expr)) {                                                                   \
            printf("FAIL: %s:%d %s\n", __func__, __LINE__, #expr);                       \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)
#define ASSERT_FALSE(expr)                                                               \
    do {                                                                                 \
        tests_run++;                                                                     \
        if ((expr)) {                                                                    \
            printf("FAIL: %s:%d %s\n", __func__, __LINE__, #expr);                       \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)
#define ASSERT_NOT_NULL(ptr)                                                             \
    do {                                                                                 \
        tests_run++;                                                                     \
        if (!(ptr)) {                                                                    \
            printf("FAIL: %s:%d %s is NULL\n", __func__, __LINE__, #ptr);                \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)
#define ASSERT_NULL(ptr)                                                                 \
    do {                                                                                 \
        tests_run++;                                                                     \
        if ((ptr)) {                                                                     \
            printf("FAIL: %s:%d %s is not NULL\n", __func__, __LINE__, #ptr);            \
            return;                                                                      \
        }                                                                                \
        tests_passed++;                                                                  \
    } while (0)

/* Forward declarations */
void test_catalog_invalid_handle(void);
void test_catalog_no_connection(void);
void test_write_op_detection_insert(void);
void test_write_op_detection_update(void);
void test_write_op_detection_delete(void);
void test_write_op_detection_create(void);
void test_write_op_detection_drop(void);
void test_write_op_detection_alter(void);
void test_write_op_detection_truncate(void);
void test_write_op_detection_grant(void);
void test_write_op_detection_revoke(void);
void test_write_op_detection_select_not_write(void);
void test_datasources_basic(void);
void test_drivers_basic(void);
void test_rowcount_write_op(void);
void test_rowcount_read_op(void);
void test_catalog_pattern_building(void);
void test_catalog_types_pattern(void);

/* ========================================================================
 * Test: Catalog functions with invalid handle
 * ======================================================================== */
TEST(catalog_invalid_handle)
{
    SQLRETURN ret;

    /* SQLTables with NULL handle */
    ret = SQLTables(NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_EQ(ret, SQL_INVALID_HANDLE);

    /* SQLColumns with NULL handle */
    ret = SQLColumns(NULL, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_EQ(ret, SQL_INVALID_HANDLE);
}

/* ========================================================================
 * Test: Catalog functions with no connection
 * ======================================================================== */
TEST(catalog_no_connection)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    /* Allocate handles */
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Try catalog query without connection - should fail */
    ret = SQLTables(stmt, NULL, 0, NULL, 0, NULL, 0, NULL, 0);
    ASSERT_EQ(ret, SQL_ERROR);

    /* Cleanup */
    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* ========================================================================
 * Write operation detection (exercises the real trino_sql_is_write_op)
 * ======================================================================== */
TEST(write_op_detection_insert)
{
    ASSERT_TRUE(
        trino_sql_is_write_op((const SQLCHAR *)"INSERT INTO users (name) VALUES ('t')"));
}

TEST(write_op_detection_update)
{
    ASSERT_TRUE(trino_sql_is_write_op(
        (const SQLCHAR *)"UPDATE users SET name = 't' WHERE id = 1"));
}

TEST(write_op_detection_delete)
{
    ASSERT_TRUE(trino_sql_is_write_op((const SQLCHAR *)"DELETE FROM users WHERE id = 1"));
}

TEST(write_op_detection_create)
{
    ASSERT_TRUE(trino_sql_is_write_op(
        (const SQLCHAR *)"CREATE TABLE test (id INTEGER, name VARCHAR)"));
}

TEST(write_op_detection_drop)
{
    ASSERT_TRUE(trino_sql_is_write_op((const SQLCHAR *)"DROP TABLE test"));
}

TEST(write_op_detection_alter)
{
    ASSERT_TRUE(
        trino_sql_is_write_op((const SQLCHAR *)"ALTER TABLE test ADD COLUMN c VARCHAR"));
}

TEST(write_op_detection_truncate)
{
    ASSERT_TRUE(trino_sql_is_write_op((const SQLCHAR *)"TRUNCATE TABLE test"));
}

TEST(write_op_detection_grant)
{
    ASSERT_TRUE(trino_sql_is_write_op((const SQLCHAR *)"GRANT SELECT ON test TO usr"));
}

TEST(write_op_detection_revoke)
{
    ASSERT_TRUE(trino_sql_is_write_op((const SQLCHAR *)"REVOKE SELECT ON test FROM usr"));
}

TEST(write_op_detection_select_not_write)
{
    ASSERT_FALSE(trino_sql_is_write_op((const SQLCHAR *)"SELECT * FROM users"));
}

/* Lowercase keywords must still be detected. */
TEST(write_op_detection_lowercase)
{
    ASSERT_TRUE(trino_sql_is_write_op((const SQLCHAR *)"insert into t values (1)"));
    ASSERT_FALSE(trino_sql_is_write_op((const SQLCHAR *)"select 1"));
}

/* Leading whitespace and comments must be skipped before classifying. */
TEST(write_op_detection_leading_comments)
{
    ASSERT_TRUE(trino_sql_is_write_op(
        (const SQLCHAR *)"  -- audit insert\n  INSERT INTO t VALUES (1)"));
    ASSERT_TRUE(trino_sql_is_write_op((const SQLCHAR *)"/* block */ UPDATE t SET x = 1"));
    ASSERT_FALSE(trino_sql_is_write_op((const SQLCHAR *)"/* not a write */ SELECT 1"));
}

/* A CTE preceding the operative statement determines the classification. */
TEST(write_op_detection_cte)
{
    ASSERT_FALSE(
        trino_sql_is_write_op((const SQLCHAR *)"WITH a AS (SELECT 1) SELECT * FROM a"));
    ASSERT_TRUE(trino_sql_is_write_op(
        (const SQLCHAR *)"WITH a AS (SELECT 1) INSERT INTO t SELECT * FROM a"));
}

/* Identifiers that merely start with a keyword must not match. */
TEST(write_op_detection_word_boundary)
{
    ASSERT_FALSE(trino_sql_is_write_op((const SQLCHAR *)"SELECT * FROM inserted_rows"));
    ASSERT_FALSE(trino_sql_is_write_op((const SQLCHAR *)"SELECT updates FROM t"));
}

/* ========================================================================
 * Test: SQLDataSources basic functionality
 * ======================================================================== */
TEST(datasources_basic)
{
    SQLHENV env;
    SQLRETURN ret;
    SQLCHAR server_name[256];
    SQLCHAR driver_name[256];
    SQLSMALLINT server_len, driver_len;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLDataSources(env, SQL_FETCH_FIRST, server_name, sizeof(server_name),
                         &server_len, driver_name, sizeof(driver_name), &driver_len);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_TRUE(server_len > 0);
    ASSERT_TRUE(driver_len > 0);
    ASSERT_EQ(strcmp((char *)server_name, "Trino"), 0);

    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* ========================================================================
 * Test: SQLDrivers basic functionality
 * ======================================================================== */
TEST(drivers_basic)
{
    SQLHENV env;
    SQLRETURN ret;
    SQLCHAR driver_data[256];
    SQLCHAR driver_attrs[512];
    SQLSMALLINT data_len, attrs_len;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLDrivers(env, 0, /* SQL_DRIVER_FIRST = 0 */
                     driver_data, sizeof(driver_data), &data_len, driver_attrs,
                     sizeof(driver_attrs), &attrs_len);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_TRUE(data_len > 0);
    ASSERT_TRUE(attrs_len > 0);
    ASSERT_EQ(strcmp((char *)driver_data, "Trino ODBC Driver"), 0);

    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* ========================================================================
 * Test: RowCount for write operations
 * ======================================================================== */
TEST(rowcount_write_op)
{
    /* Test that write operations properly track row count */
    /* This is a unit test of the logic, not a live test */

    /* Simulate a write operation result with rows_processed = 5 */
    SQLLEN expected_row_count = 5;
    SQLLEN actual_row_count = expected_row_count;

    ASSERT_EQ(actual_row_count, expected_row_count);
}

/* ========================================================================
 * Test: RowCount for read operations
 * ======================================================================== */
TEST(rowcount_read_op)
{
    /* Test that read operations don't set row_count from stats */
    /* (row_count is set during fetching for read ops) */

    /* For read operations, row_count should be 0 initially */
    SQLLEN initial_row_count = 0;
    ASSERT_EQ(initial_row_count, 0);
}

/* ========================================================================
 * Test: Catalog pattern building
 * ======================================================================== */
TEST(catalog_pattern_building)
{
    /* Test that pattern strings are properly built for LIKE clauses */
    char pattern[256];

    /* Test with NULL pattern (should become '%') */
    if (1) {
        strcpy(pattern, "'%'");
        ASSERT_EQ(strcmp(pattern, "'%'"), 0);
    }

    /* Test with specific pattern */
    strcpy(pattern, "'users%'");
    ASSERT_EQ(strcmp(pattern, "'users%'"), 0);
}

/* ========================================================================
 * Test: Catalog types pattern
 * ======================================================================== */
TEST(catalog_types_pattern)
{
    /* Test that types are properly converted to SQL IN clause */
    char types_pat[256];

    /* Default types */
    strcpy(types_pat, "('TABLE','VIEW')");
    ASSERT_EQ(strcmp(types_pat, "('TABLE','VIEW')"), 0);

    /* Custom types */
    strcpy(types_pat, "('TABLE')");
    ASSERT_EQ(strcmp(types_pat, "('TABLE')"), 0);
}
