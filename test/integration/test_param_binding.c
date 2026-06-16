/* Unit tests for parameter binding */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trino_odbc.h"
#include "trino_odbc/statement.h"

int tests_run = 0;
int tests_passed = 0;

#define TEST(name) void test_##name(void)
#define ASSERT_EQ(a, b) do { tests_run++; if ((a) != (b)) { printf("FAIL: %s:%d %s != %s\n", __func__, __LINE__, #a, #b); return; } tests_passed++; } while(0)

/* Test: Parameter binding - CHAR type */
TEST(param_bind_char)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Prepare a statement with one parameter */
    ret = SQLPrepare(stmt, (SQLCHAR *)"SELECT ?", SQL_NTS);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Bind a CHAR parameter */
    char *param_value = "test";
    SQLLEN str_len = SQL_NTS;
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           50, 0, param_value, strlen(param_value) + 1, &str_len);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* Test: Parameter binding - LONG type */
TEST(param_bind_long)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Prepare a statement with one parameter */
    ret = SQLPrepare(stmt, (SQLCHAR *)"SELECT ?", SQL_NTS);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Bind a LONG parameter */
    long param_value = 12345;
    SQLLEN str_len_or_ind = 0;
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER,
                           0, 0, &param_value, 0, &str_len_or_ind);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* Test: Parameter binding - INT type */
TEST(param_bind_int)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Prepare a statement with one parameter */
    ret = SQLPrepare(stmt, (SQLCHAR *)"SELECT ?", SQL_NTS);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Bind an INT parameter */
    int param_value = 42;
    SQLLEN str_len_or_ind = 0;
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_INT, SQL_INTEGER,
                           0, 0, &param_value, 0, &str_len_or_ind);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* Test: Parameter binding - FLOAT type */
TEST(param_bind_float)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Prepare a statement with one parameter */
    ret = SQLPrepare(stmt, (SQLCHAR *)"SELECT ?", SQL_NTS);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Bind a FLOAT parameter */
    float param_value = 3.14f;
    SQLLEN str_len_or_ind = 0;
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_FLOAT, SQL_REAL,
                           0, 0, &param_value, 0, &str_len_or_ind);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* Test: Parameter binding - DOUBLE type */
TEST(param_bind_double)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Prepare a statement with one parameter */
    ret = SQLPrepare(stmt, (SQLCHAR *)"SELECT ?", SQL_NTS);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Bind a DOUBLE parameter */
    double param_value = 2.71828;
    SQLLEN str_len_or_ind = 0;
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_DOUBLE, SQL_DOUBLE,
                           0, 0, &param_value, 0, &str_len_or_ind);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* Test: Parameter binding - NULL value */
TEST(param_bind_null)
{
    SQLHENV env;
    SQLHDBC dbc;
    SQLHSTMT stmt;
    SQLRETURN ret;

    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    ASSERT_EQ(ret, SQL_SUCCESS);

    ret = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Prepare a statement with one parameter */
    ret = SQLPrepare(stmt, (SQLCHAR *)"SELECT ?", SQL_NTS);
    ASSERT_EQ(ret, SQL_SUCCESS);

    /* Bind a NULL parameter */
    SQLLEN str_len_or_ind = SQL_NULL_DATA;
    ret = SQLBindParameter(stmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR,
                           0, 0, NULL, 0, &str_len_or_ind);
    ASSERT_EQ(ret, SQL_SUCCESS);

    SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);
}

/* This is the single main for parameter binding tests */
int main(void)
{
    printf("Running Trino ODBC Driver parameter binding unit tests...\n\n");

    /* Parameter binding tests */
    test_param_bind_char();
    test_param_bind_long();
    test_param_bind_int();
    test_param_bind_float();
    test_param_bind_double();
    test_param_bind_null();

    printf("\n========================================\n");
    printf("Tests run:    %d\n", tests_run);
    printf("Tests passed: %d\n", tests_passed);
    printf("Tests failed: %d\n", tests_run - tests_passed);

    if (tests_passed == tests_run) {
        printf("\nAll parameter binding tests PASSED!\n");
        return 0;
    } else {
        printf("\nSome parameter binding tests FAILED.\n");
        return 1;
    }
}
