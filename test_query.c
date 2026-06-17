/* Simple ODBC test program to verify Unicode entry points and query execution.
 * Compile on Windows with: cl test_query.c /I"C:\path\to\include" odbc32.lib
 */

#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK_ERROR(rc, htype, handle, msg) \
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) { \
        print_error(htype, handle, msg); \
        goto cleanup; \
    }

void print_error(SQLSMALLINT handle_type, SQLHANDLE handle, const char *msg) {
    SQLWCHAR sqlstate[6];
    SQLWCHAR message[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER native_error;
    SQLSMALLINT text_length;
    
    printf("ERROR: %s\n", msg);
    
    SQLSMALLINT i = 1;
    while (SQLGetDiagRecW(handle_type, handle, i, sqlstate, &native_error,
                          message, sizeof(message)/sizeof(SQLWCHAR), &text_length) == SQL_SUCCESS) {
        wprintf(L"  [%s] %s (Native: %d)\n", sqlstate, message, native_error);
        i++;
    }
}

int main(int argc, char *argv[]) {
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    SQLHSTMT stmt = SQL_NULL_HSTMT;
    SQLRETURN rc;
    
    // Connection string - modify as needed
    SQLWCHAR conn_str[] = L"DRIVER={Trino ODBC Driver};"
                          L"Server=trino-dev.harlandclarke.local;"
                          L"Port=443;"
                          L"SSL=true;"
                          L"SSLVerify=false;"
                          L"SSLNoRevoke=true;"
                          L"Catalog=tpch;"
                          L"Schema=tiny;"
                          L"User=a002687;"
                          L"Password=YOUR_PASSWORD;"
                          L"Authentication=PASSWORD;";
    
    printf("=== ODBC Unicode Test ===\n\n");
    
    // 1. Allocate environment handle
    printf("1. Allocating environment handle...\n");
    rc = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    CHECK_ERROR(rc, SQL_HANDLE_ENV, env, "Failed to allocate environment");
    printf("   OK: env=%p\n\n", env);
    
    // 2. Set ODBC version
    printf("2. Setting ODBC version to 3.x...\n");
    rc = SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    CHECK_ERROR(rc, SQL_HANDLE_ENV, env, "Failed to set ODBC version");
    printf("   OK\n\n");
    
    // 3. Allocate connection handle
    printf("3. Allocating connection handle...\n");
    rc = SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    CHECK_ERROR(rc, SQL_HANDLE_DBC, dbc, "Failed to allocate connection");
    printf("   OK: dbc=%p\n\n", dbc);
    
    // 4. Connect to database
    printf("4. Connecting to database...\n");
    SQLWCHAR out_conn[1024];
    SQLSMALLINT out_len;
    rc = SQLDriverConnectW(dbc, NULL, conn_str, SQL_NTS, out_conn, 
                          sizeof(out_conn)/sizeof(SQLWCHAR), &out_len, SQL_DRIVER_NOPROMPT);
    CHECK_ERROR(rc, SQL_HANDLE_DBC, dbc, "Failed to connect");
    printf("   OK: Connected\n\n");
    
    // 5. Allocate statement handle
    printf("5. Allocating statement handle (dbc=%p)...\n", dbc);
    rc = SQLAllocHandle(SQL_HANDLE_STMT, dbc, &stmt);
    printf("   SQLAllocHandle returned: %d (stmt=%p)\n", rc, stmt);
    if (rc != SQL_SUCCESS && rc != SQL_SUCCESS_WITH_INFO) {
        print_error(SQL_HANDLE_DBC, dbc, "Failed to allocate statement");
        goto cleanup;
    }
    printf("   OK: stmt=%p\n\n", stmt);
    
    // 6. Set query timeout (tests SQLSetStmtAttrW)
    printf("6. Setting query timeout...\n");
    fflush(stdout);
    printf("   About to call SQLSetStmtAttr...\n");
    fflush(stdout);
    rc = SQLSetStmtAttr(stmt, SQL_ATTR_QUERY_TIMEOUT, (SQLPOINTER)30, 0);
    printf("   SQLSetStmtAttr returned: %d\n", rc);
    fflush(stdout);
    CHECK_ERROR(rc, SQL_HANDLE_STMT, stmt, "Failed to set query timeout");
    printf("   OK\n\n");
    
    // 7. Execute query
    printf("7. Executing query: SELECT 1 AS one...\n");
    SQLWCHAR query[] = L"SELECT 1 AS one";
    rc = SQLExecDirectW(stmt, query, SQL_NTS);
    CHECK_ERROR(rc, SQL_HANDLE_STMT, stmt, "Failed to execute query");
    printf("   OK: Query executed\n\n");
    
    // 8. Fetch result
    printf("8. Fetching result...\n");
    rc = SQLFetch(stmt);
    CHECK_ERROR(rc, SQL_HANDLE_STMT, stmt, "Failed to fetch");
    printf("   OK: Row fetched\n\n");
    
    // 9. Get data
    printf("9. Getting column data...\n");
    SQLINTEGER value;
    SQLLEN indicator;
    rc = SQLGetData(stmt, 1, SQL_C_SLONG, &value, sizeof(value), &indicator);
    CHECK_ERROR(rc, SQL_HANDLE_STMT, stmt, "Failed to get data");
    printf("   OK: Value = %d\n\n", value);
    
    printf("=== TEST PASSED ===\n");
    
cleanup:
    printf("\n=== CLEANUP ===\n");
    fflush(stdout);
    if (stmt != SQL_NULL_HSTMT) {
        printf("Freeing statement handle...\n");
        fflush(stdout);
        SQLFreeHandle(SQL_HANDLE_STMT, stmt);
    }
    if (dbc != SQL_NULL_HDBC) {
        printf("Disconnecting...\n");
        fflush(stdout);
        SQLDisconnect(dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    }
    if (env != SQL_NULL_HENV) {
        printf("Freeing environment handle...\n");
        fflush(stdout);
        SQLFreeHandle(SQL_HANDLE_ENV, env);
    }
    
    printf("Exiting with code 0\n");
    fflush(stdout);
    return 0;
}
