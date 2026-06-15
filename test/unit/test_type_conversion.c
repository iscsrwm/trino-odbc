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

TEST(trino_to_odbc_types)
{
    ASSERT_EQ(trino_type_to_odbc_type("varchar"), SQL_VARCHAR);
    ASSERT_EQ(trino_type_to_odbc_type("char"), SQL_CHAR);
    ASSERT_EQ(trino_type_to_odbc_type("varbinary"), SQL_VARBINARY);
    ASSERT_EQ(trino_type_to_odbc_type("boolean"), SQL_BIT);
    ASSERT_EQ(trino_type_to_odbc_type("tinyint"), SQL_TINYINT);
    ASSERT_EQ(trino_type_to_odbc_type("smallint"), SQL_SMALLINT);
    ASSERT_EQ(trino_type_to_odbc_type("integer"), SQL_INTEGER);
    ASSERT_EQ(trino_type_to_odbc_type("bigint"), SQL_BIGINT);
    ASSERT_EQ(trino_type_to_odbc_type("real"), SQL_REAL);
    ASSERT_EQ(trino_type_to_odbc_type("double"), SQL_DOUBLE);
    ASSERT_EQ(trino_type_to_odbc_type("decimal"), SQL_DECIMAL);
    ASSERT_EQ(trino_type_to_odbc_type("date"), SQL_TYPE_DATE);
    ASSERT_EQ(trino_type_to_odbc_type("time"), SQL_TYPE_TIME);
    ASSERT_EQ(trino_type_to_odbc_type("timestamp"), SQL_TYPE_TIMESTAMP);
    ASSERT_EQ(trino_type_to_odbc_type("json"), SQL_VARCHAR);
    ASSERT_EQ(trino_type_to_odbc_type("array"), SQL_VARCHAR);
    ASSERT_EQ(trino_type_to_odbc_type("map"), SQL_VARCHAR);
    ASSERT_EQ(trino_type_to_odbc_type("row"), SQL_VARCHAR);
    ASSERT_EQ(trino_type_to_odbc_type("uuid"), SQL_GUID);
}

TEST(type_name_lookup)
{
    ASSERT_EQ(strcmp(trino_type_name(SQL_VARCHAR), "VARCHAR"), 0);
    ASSERT_EQ(strcmp(trino_type_name(SQL_INTEGER), "INTEGER"), 0);
    ASSERT_EQ(strcmp(trino_type_name(SQL_BIGINT), "BIGINT"), 0);
    ASSERT_EQ(strcmp(trino_type_name(SQL_DOUBLE), "DOUBLE"), 0);
    ASSERT_EQ(strcmp(trino_type_name(SQL_TYPE_DATE), "DATE"), 0);
    ASSERT_EQ(strcmp(trino_type_name(SQL_TYPE_TIMESTAMP), "TIMESTAMP"), 0);
}

/* main() is in test_connection_string.c — it calls all test functions */
