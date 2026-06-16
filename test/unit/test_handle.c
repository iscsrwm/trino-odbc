#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trino_odbc.h"
#include "trino_odbc/core.h"
#include "trino_odbc/error.h"
#include "trino_odbc/protocol.h"

/* Shared test counters — defined in test_connection_string.c */
int tests_run = 0;
int tests_passed = 0;

#define TEST(name) void test_##name(void)
#define ASSERT_EQ(a, b) do { tests_run++; if ((a) != (b)) { printf("FAIL: %s:%d %s != %s\n", __func__, __LINE__, #a, #b); return; } tests_passed++; } while(0)
#define ASSERT_TRUE(expr) do { tests_run++; if (!(expr)) { printf("FAIL: %s:%d %s\n", __func__, __LINE__, #expr); return; } tests_passed++; } while(0)
#define ASSERT_FALSE(expr) do { tests_run++; if ((expr)) { printf("FAIL: %s:%d %s\n", __func__, __LINE__, #expr); return; } tests_passed++; } while(0)
#define ASSERT_NOT_NULL(ptr) do { tests_run++; if (!(ptr)) { printf("FAIL: %s:%d %s is NULL\n", __func__, __LINE__, #ptr); return; } tests_passed++; } while(0)
#define ASSERT_NULL(ptr) do { tests_run++; if ((ptr)) { printf("FAIL: %s:%d %s is not NULL\n", __func__, __LINE__, #ptr); return; } tests_passed++; } while(0)

/* ========================================================================
 * Test: Diagnostics initialization
 * ======================================================================== */
TEST(diag_init)
{
    trino_diagnostics_t diag;
    trino_diag_init(&diag);

    ASSERT_EQ(diag.record_count, 0);
    ASSERT_EQ(diag.rec_number, 0);
    ASSERT_EQ(memcmp(diag.sqlstate, "00000", 5), 0);
}

/* ========================================================================
 * Test: Diagnostics add record
 * ======================================================================== */
TEST(diag_add_record)
{
    trino_diagnostics_t diag;
    trino_diag_init(&diag);

    trino_diag_add(&diag, "42000", 1234, "Test error message");

    ASSERT_EQ(diag.record_count, 1);
    ASSERT_EQ(memcmp(diag.records[0].sqlstate, "42000", 5), 0);
    ASSERT_EQ(diag.records[0].native_error, 1234);
    ASSERT_EQ(strcmp((char *)diag.records[0].message_text, "Test error message"), 0);
}

/* ========================================================================
 * Test: Diagnostics clear
 * ======================================================================== */
TEST(diag_clear)
{
    trino_diagnostics_t diag;
    trino_diag_init(&diag);

    trino_diag_add(&diag, "42000", 1234, "Test error");
    ASSERT_EQ(diag.record_count, 1);

    trino_diag_clear(&diag);
    ASSERT_EQ(diag.record_count, 0);
    ASSERT_EQ(memcmp(diag.sqlstate, "00000", 5), 0);
}

/* ========================================================================
 * Test: Handle allocation - environment
 * ======================================================================== */
TEST(handle_alloc_env)
{
    SQLHENV env;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);

    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_NOT_NULL(env);
}

/* ========================================================================
 * Test: Handle allocation - connection
 * ======================================================================== */
TEST(handle_alloc_conn)
{
    SQLHENV env;
    SQLHDBC dbc;

    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_NOT_NULL(dbc);
}

/* ========================================================================
 * Test: Handle allocation - statement
 * ======================================================================== */
TEST(handle_alloc_stmt)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;

    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);

    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_NOT_NULL(stmt);
}

/* ========================================================================
 * Test: Handle free
 * ======================================================================== */
TEST(handle_free)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;

    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);

    SQLRETURN ret = SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLFreeHandle(SQL_HANDLE_ENV, env);
    ASSERT_EQ(ret, SQL_SUCCESS);
}

/* ========================================================================
 * Test: Invalid handle
 * ======================================================================== */
TEST(handle_invalid)
{
    SQLHENV env;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, (SQLHANDLE)0xdeadbeef, &env);
    ASSERT_EQ(ret, SQL_INVALID_HANDLE);
}

/* ========================================================================
 * Test: Environment attributes
 * ======================================================================== */
TEST(env_attributes)
{
    SQLHENV env;
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);

    SQLUINTEGER val = SQL_OV_ODBC3;
    SQLRETURN ret = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, &val, 0);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLUINTEGER get_val = 0;
    ret = SQLGetEnvAttr(env, SQL_ATTR_ODBC_VERSION, &get_val, sizeof(get_val), NULL);
    ASSERT_EQ(ret, SQL_SUCCESS);
    ASSERT_EQ(get_val, SQL_OV_ODBC3);
}

/* ========================================================================
 * Test: Type mapping
 * ======================================================================== */
TEST(type_mapping)
{
    ASSERT_EQ(trino_type_to_odbc_type("varchar"), SQL_VARCHAR);
    ASSERT_EQ(trino_type_to_odbc_type("varchar(100)"), SQL_VARCHAR);
    ASSERT_EQ(trino_type_to_odbc_type("integer"), SQL_INTEGER);
    ASSERT_EQ(trino_type_to_odbc_type("bigint"), SQL_BIGINT);
    ASSERT_EQ(trino_type_to_odbc_type("double"), SQL_DOUBLE);
    ASSERT_EQ(trino_type_to_odbc_type("boolean"), SQL_BIT);
    ASSERT_EQ(trino_type_to_odbc_type("date"), SQL_TYPE_DATE);
    ASSERT_EQ(trino_type_to_odbc_type("timestamp"), SQL_TYPE_TIMESTAMP);
    ASSERT_EQ(trino_type_to_odbc_type("decimal(10,2)"), SQL_DECIMAL);
    ASSERT_EQ(trino_type_to_odbc_type("unknown_type"), SQL_VARCHAR);
}

/* main() is in test_connection_string.c — it calls all test functions */
