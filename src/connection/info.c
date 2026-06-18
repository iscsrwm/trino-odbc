/* SQLGetInfo and SQLGetFunctions implementation */

#include "trino_odbc/connection.h"
#include "trino_odbc/protocol.h"
#include "trino_odbc/log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ODBC function bitmap macros (from sqlext.h, defined here for portability) */
#ifndef SQL_API_ODBC3_ALL_FUNCTIONS_SIZE
#define SQL_API_ODBC3_ALL_FUNCTIONS_SIZE 250
#endif

#ifndef SQL_FUNC_EXISTS
#define SQL_FUNC_EXISTS(pfExists, uwAPI) \
    ((*(((UWORD*)(pfExists)) + ((uwAPI) >> 4)) & (1 << ((uwAPI) & 0x000F))) ? SQL_TRUE : SQL_FALSE)
#endif

/* Macro to SET a function as supported in the bitmap */
#define SQL_FUNC_SET(pfExists, uwAPI) \
    (*(((UWORD*)(pfExists)) + ((uwAPI) >> 4)) |= (1 << ((uwAPI) & 0x000F)))

/* ========================================================================
 * SQLGetInfo - Returns information about the data source and driver
 * ======================================================================== */

SQLRETURN SQLGetInfo(SQLHDBC connection_handle, SQLUSMALLINT info_type,
                     SQLPOINTER char_attr, SQLSMALLINT buffer_length,
                     SQLSMALLINT *str_len)
{
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    if (!char_attr)
        return SQL_INVALID_HANDLE;

    switch (info_type) {
        case SQL_ACCESSIBLE_TABLES:
            strncpy((char *)char_attr, "N", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_ACCESSIBLE_PROCEDURES:
            strncpy((char *)char_attr, "N", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_BATCH_SUPPORT:
            *(SQLUINTEGER *)char_attr = 0;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_CATALOG_LOCATION:
            *(SQLUSMALLINT *)char_attr = SQL_CL_START;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_CATALOG_NAME:
            strncpy((char *)char_attr, "Y", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_CATALOG_TERM:
            strncpy((char *)char_attr, "catalog", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_CATALOG_USAGE:
            *(SQLUINTEGER *)char_attr = SQL_CU_DML_STATEMENTS | SQL_CU_TABLE_DEFINITION;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_CONCAT_NULL_BEHAVIOR:
            *(SQLUSMALLINT *)char_attr = SQL_CB_NULL;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_CURSOR_COMMIT_BEHAVIOR:
            *(SQLUSMALLINT *)char_attr = SQL_CB_CLOSE;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_CURSOR_ROLLBACK_BEHAVIOR:
            *(SQLUSMALLINT *)char_attr = SQL_CB_CLOSE;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_DATA_SOURCE_NAME:
            if (conn->server) {
                snprintf((char *)char_attr, (size_t)buffer_length, "%s:%d", conn->server,
                         conn->port);
            } else {
                strncpy((char *)char_attr, "Trino", (size_t)buffer_length - 1);
            }
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_DBMS_NAME:
            strncpy((char *)char_attr, "Trino", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_DBMS_VER:
            /* We don't know the server version until we connect */
            strncpy((char *)char_attr, "357.1", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_DEFAULT_TXN_ISOLATION:
            *(SQLUINTEGER *)char_attr = SQL_TXN_READ_COMMITTED;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_DRIVER_NAME:
            strncpy((char *)char_attr, "TrinoODBC", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_DRIVER_VER:
            strncpy((char *)char_attr, TRINO_ODBC_VERSION_STR, (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_DRIVER_ODBC_VER:
            strncpy((char *)char_attr, "03.80", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_FILE_USAGE:
            *(SQLUSMALLINT *)char_attr = SQL_FILE_NOT_SUPPORTED;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_GETDATA_EXTENSIONS:
            *(SQLUINTEGER *)char_attr = SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_IDENTIFIER_QUOTE_CHAR:
            strncpy((char *)char_attr, "\"", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_KEYWORDS:
            strncpy((char *)char_attr,
                    "ADD,ALL,ALTER,AND,ANY,AS,ASC,AT,AUTHORIZATION,"
                    "BETWEEN,BOTH,BY,CASE,CASCADE,CAST,CHECK,COALESCE,COLLATE,"
                    "COLUMN,COMMIT,CONNECT,COPY,"
                    "CREATE,CROSS,CUBE,CURRENT,CURRENT_DATE,CURRENT_TIME,"
                    "CURRENT_TIMESTAMP,CURRENT_USER,CYCLE,DATA,DATE,DAY,DEALLOCATE,"
                    "DEC,DECIMAL,DECLARE,DEFAULT,DELETE,DESC,DETERMINISTIC,"
                    "DISTINCT,DOUBLE,DROP,ELSE,END,EXCEPT,"
                    "EXECUTE,EXISTS,EXPLAIN,EXTERNAL,EXTRACT,FALSE,"
                    "FETCH,FIRST,FLOAT,FOR,FOREIGN,FROM,FULL,FUNCTION,"
                    "FUNCTIONS,GRANT,GROUP,GROUPING,GROUPS,HAVING,HOUR,"
                    "IN,INNER,INSERT,INT,"
                    "INTEGER,INTERSECT,INTERVAL,INTO,IS,ISOLATION,"
                    "JOIN,LATERAL,LEADING,LEFT,LIKE,LIMIT,LOCALTIME,"
                    "LOCALTIMESTAMP,MERGE,MINUTE,"
                    "MONTH,NATURAL,NEXT,NO,NONE,NOT,"
                    "NULL,NULLIF,NUMERIC,OF,OFFSET,ON,ONLY,"
                    "OR,ORDER,OUTER,OVER,PARTITION,"
                    "PRECEDING,PRIMARY,"
                    "PROCEDURE,RANGE,READ,REAL,RECURSIVE,REFERENCES,RELEASE,"
                    "RENAME,REPEATABLE,REPLACE,RESTRICT,RETURNING,REVOKE,RIGHT,"
                    "ROLLBACK,ROLLUP,ROW,ROWS,SCHEMA,SELECT,SESSION_USER,"
                    "SET,SETS,SHOW,SOME,START,SYSTEM,"
                    "TABLE,TABLESAMPLE,THEN,TIES,TIME,TIMESTAMP,TO,TRAILING,TRANSACTION,"
                    "TRUE,TRUNCATE,TYPE,UNBOUNDED,UNION,UNIQUE,"
                    "UNKNOWN,UPDATE,USER,VALUES,VIEW,"
                    "WHEN,WHERE,WINDOW,WITH,WORK,WRITE,YEAR,ZONE",
                    (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_LIKE_ESCAPE_CLAUSE:
            strncpy((char *)char_attr, "Y", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_MAX_CATALOG_NAME_LEN:
            *(SQLUSMALLINT *)char_attr = TRINO_MAX_IDENTIFIER_LEN;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_COLUMN_NAME_LEN:
            *(SQLUSMALLINT *)char_attr = TRINO_MAX_IDENTIFIER_LEN;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_COLUMNS_IN_GROUP_BY:
        case SQL_MAX_COLUMNS_IN_INDEX:
        case SQL_MAX_COLUMNS_IN_ORDER_BY:
        case SQL_MAX_COLUMNS_IN_SELECT:
        case SQL_MAX_COLUMNS_IN_TABLE:
            *(SQLUSMALLINT *)char_attr = 0; /* 0 = no limit / unknown */
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_DRIVER_CONNECTIONS:
            *(SQLUSMALLINT *)char_attr = 0; /* 0 = no limit */
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_CURSOR_NAME_LEN:
            *(SQLUSMALLINT *)char_attr = TRINO_MAX_IDENTIFIER_LEN;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_IDENTIFIER_LEN:
            *(SQLUSMALLINT *)char_attr = TRINO_MAX_IDENTIFIER_LEN;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_INDEX_SIZE:
            *(SQLUINTEGER *)char_attr = 0;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_SCHEMA_NAME_LEN:
            *(SQLUSMALLINT *)char_attr = TRINO_MAX_IDENTIFIER_LEN;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_TABLE_NAME_LEN:
            *(SQLUSMALLINT *)char_attr = TRINO_MAX_IDENTIFIER_LEN;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_MAX_TABLES_IN_SELECT:
            *(SQLUSMALLINT *)char_attr = 0;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_NEED_LONG_DATA_LEN:
            strncpy((char *)char_attr, "N", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_NON_NULLABLE_COLUMNS:
            *(SQLUSMALLINT *)char_attr = SQL_NNC_NON_NULL;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_NULL_COLLATION:
            *(SQLUSMALLINT *)char_attr = SQL_NC_START;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_ODBC_INTERFACE_CONFORMANCE:
            *(SQLUINTEGER *)char_attr = SQL_OIC_CORE;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_OJ_CAPABILITIES:
            *(SQLUINTEGER *)char_attr =
                SQL_OJ_LEFT | SQL_OJ_RIGHT | SQL_OJ_FULL | SQL_OJ_NESTED;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_ORDER_BY_COLUMNS_IN_SELECT:
            strncpy((char *)char_attr, "Y", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_PROCEDURES:
            strncpy((char *)char_attr, "N", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = 1;
            break;

        case SQL_QUOTED_IDENTIFIER_CASE:
            *(SQLUSMALLINT *)char_attr = SQL_IC_SENSITIVE;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUSMALLINT);
            break;

        case SQL_SCHEMA_TERM:
            strncpy((char *)char_attr, "schema", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_SERVER_NAME:
            if (conn->server) {
                strncpy((char *)char_attr, conn->server, (size_t)buffer_length - 1);
                ((char *)char_attr)[buffer_length - 1] = '\0';
                if (str_len)
                    *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            } else {
                ((char *)char_attr)[0] = '\0';
                if (str_len)
                    *str_len = 0;
            }
            break;

        case SQL_SPECIAL_CHARACTERS:
            strncpy((char *)char_attr, "_", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        case SQL_TXN_ISOLATION_OPTION:
            *(SQLUINTEGER *)char_attr =
                SQL_TXN_READ_COMMITTED | SQL_TXN_REPEATABLE_READ | SQL_TXN_SERIALIZABLE;
            if (str_len)
                *str_len = (SQLSMALLINT)sizeof(SQLUINTEGER);
            break;

        case SQL_USER_NAME:
            if (conn->user) {
                strncpy((char *)char_attr, conn->user, (size_t)buffer_length - 1);
                ((char *)char_attr)[buffer_length - 1] = '\0';
                if (str_len)
                    *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            } else {
                ((char *)char_attr)[0] = '\0';
                if (str_len)
                    *str_len = 0;
            }
            break;

        case SQL_XOPEN_CLI_YEAR:
            strncpy((char *)char_attr, "2019", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len)
                *str_len = (SQLSMALLINT)strlen((char *)char_attr);
            break;

        default:
            /* Unknown info type - return empty */
            if (str_len)
                *str_len = 0;
            break;
    }

    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLGetInfoW - Unicode variant of SQLGetInfo.
 *
 * CRITICAL: once the driver exports any W entry point, the Windows ODBC Driver
 * Manager treats the driver as Unicode and calls SQLGetInfoW (not SQLGetInfo)
 * during connection setup. If this is missing, SQLDriverConnectW appears to
 * succeed but the DM's post-connect capability probe fails and Open() aborts
 * with an empty error message.
 *
 * For string-valued info types we widen the ANSI result to UTF-16. For the W
 * variants, BufferLength and *StringLength are expressed in BYTES (an ODBC
 * quirk), not characters. Numeric/bitmask info types are width-agnostic and are
 * delegated straight to the ANSI implementation.
 * ======================================================================== */

/* Info types that return a character string (everything else is numeric). */
static int info_type_is_string(SQLUSMALLINT t)
{
    switch (t) {
        case SQL_ACCESSIBLE_TABLES:
        case SQL_ACCESSIBLE_PROCEDURES:
        case SQL_CATALOG_NAME:
        case SQL_CATALOG_TERM:
        case SQL_DATA_SOURCE_NAME:
        case SQL_DBMS_NAME:
        case SQL_DBMS_VER:
        case SQL_DRIVER_NAME:
        case SQL_DRIVER_VER:
        case SQL_DRIVER_ODBC_VER:
        case SQL_IDENTIFIER_QUOTE_CHAR:
        case SQL_KEYWORDS:
        case SQL_LIKE_ESCAPE_CLAUSE:
        case SQL_NEED_LONG_DATA_LEN:
        case SQL_ORDER_BY_COLUMNS_IN_SELECT:
        case SQL_PROCEDURES:
        case SQL_SCHEMA_TERM:
        case SQL_SERVER_NAME:
        case SQL_SPECIAL_CHARACTERS:
        case SQL_USER_NAME:
        case SQL_XOPEN_CLI_YEAR: return 1;
        default: return 0;
    }
}

SQLRETURN SQLGetInfoW(SQLHDBC connection_handle, SQLUSMALLINT info_type,
                      SQLPOINTER info_value, SQLSMALLINT buffer_length,
                      SQLSMALLINT *str_len)
{
    trino_log("SQLGetInfoW: info_type=%u buffer_length=%d", (unsigned)info_type,
              (int)buffer_length);
    if (!info_type_is_string(info_type)) {
        /* Numeric info types are identical in both APIs. */
        return SQLGetInfo(connection_handle, info_type, info_value, buffer_length,
                          str_len);
    }

    /* Fetch the ANSI string into a local buffer, then widen to UTF-16. */
    char ansi[1024] = {0};
    SQLSMALLINT ansi_len = 0;
    SQLRETURN ret =
        SQLGetInfo(connection_handle, info_type, ansi, (SQLSMALLINT)sizeof(ansi),
                   &ansi_len);
    if (ret == SQL_ERROR || ret == SQL_INVALID_HANDLE)
        return ret;

    size_t wlen = 0;
    SQLWCHAR *w = trino_utf8_to_wchars(ansi, &wlen);

    /* W-variant lengths are in BYTES. */
    SQLSMALLINT total_bytes = (SQLSMALLINT)(wlen * sizeof(SQLWCHAR));

    if (info_value && buffer_length > 0) {
        size_t max_wchars = (size_t)buffer_length / sizeof(SQLWCHAR);
        if (max_wchars == 0)
            max_wchars = 1;
        size_t copy = wlen;
        if (copy > max_wchars - 1)
            copy = max_wchars - 1;
        SQLWCHAR *out = (SQLWCHAR *)info_value;
        if (w && copy > 0)
            memcpy(out, w, copy * sizeof(SQLWCHAR));
        out[copy] = 0;
        if ((size_t)wlen > copy)
            ret = SQL_SUCCESS_WITH_INFO;
    }
    if (str_len)
        *str_len = total_bytes;

    free(w);
    return ret;
}

/* ========================================================================
 * SQLGetFunctions - reports whether a given ODBC function is supported
 *
 * Per the ODBC spec, when FunctionId is a specific SQL_API_* code the driver
 * writes SQL_TRUE/SQL_FALSE to *Supported. SQL_API_ODBC3_ALL_FUNCTIONS and
 * SQL_API_ALL_FUNCTIONS (array forms) are not implemented here.
 * ======================================================================== */

SQLRETURN SQLGetFunctions(SQLHDBC connection_handle, SQLUSMALLINT function_id,
                          SQLUSMALLINT *supported)
{
    if (!connection_handle || !supported)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    trino_log("SQLGetFunctions: function_id=%d", (int)function_id);

    /* SQL_API_ODBC3_ALL_FUNCTIONS: the DM passes an array of SQL_API_ODBC3_ALL_FUNCTIONS_SIZE
     * USMALLINTs and expects the driver to set bits for each supported function. */
    if (function_id == SQL_API_ODBC3_ALL_FUNCTIONS) {
        SQLUSMALLINT *array = supported;
        memset(array, 0, sizeof(SQLUSMALLINT) * SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);
        
        /* Set bits for supported functions using SQL_FUNC_SET macro */
        #define MARK_SUPPORTED(fid) SQL_FUNC_SET(array, fid)
        
        MARK_SUPPORTED(SQL_API_SQLALLOCHANDLE);
        MARK_SUPPORTED(SQL_API_SQLFREEHANDLE);
        MARK_SUPPORTED(SQL_API_SQLBINDCOL);
        MARK_SUPPORTED(SQL_API_SQLBINDPARAMETER);
        MARK_SUPPORTED(SQL_API_SQLCANCEL);
        MARK_SUPPORTED(SQL_API_SQLCOLATTRIBUTE);
        MARK_SUPPORTED(SQL_API_SQLDESCRIBECOL);
        MARK_SUPPORTED(SQL_API_SQLCOLUMNS);
        MARK_SUPPORTED(SQL_API_SQLCONNECT);
        MARK_SUPPORTED(SQL_API_SQLDISCONNECT);
        MARK_SUPPORTED(SQL_API_SQLDRIVERCONNECT);
        MARK_SUPPORTED(SQL_API_SQLEXECDIRECT);
        MARK_SUPPORTED(SQL_API_SQLEXECUTE);
        MARK_SUPPORTED(SQL_API_SQLFETCH);
        MARK_SUPPORTED(SQL_API_SQLFETCHSCROLL);
        MARK_SUPPORTED(SQL_API_SQLGETDATA);
        MARK_SUPPORTED(SQL_API_SQLGETDIAGFIELD);
        MARK_SUPPORTED(SQL_API_SQLGETDIAGREC);
        MARK_SUPPORTED(SQL_API_SQLGETINFO);
        MARK_SUPPORTED(SQL_API_SQLGETFUNCTIONS);
        MARK_SUPPORTED(SQL_API_SQLNUMRESULTCOLS);
        MARK_SUPPORTED(SQL_API_SQLNUMPARAMS);
        MARK_SUPPORTED(SQL_API_SQLPREPARE);
        MARK_SUPPORTED(SQL_API_SQLROWCOUNT);
        MARK_SUPPORTED(SQL_API_SQLMORERESULTS);
        MARK_SUPPORTED(SQL_API_SQLGETCONNECTATTR);
        MARK_SUPPORTED(SQL_API_SQLSETCONNECTATTR);
        MARK_SUPPORTED(SQL_API_SQLGETSTMTATTR);
        MARK_SUPPORTED(SQL_API_SQLSETSTMTATTR);
        MARK_SUPPORTED(SQL_API_SQLGETENVATTR);
        MARK_SUPPORTED(SQL_API_SQLSETENVATTR);
        MARK_SUPPORTED(SQL_API_SQLTABLES);
        MARK_SUPPORTED(SQL_API_SQLPRIMARYKEYS);
        MARK_SUPPORTED(SQL_API_SQLSTATISTICS);
        MARK_SUPPORTED(SQL_API_SQLSPECIALCOLUMNS);
        
        #undef MARK_SUPPORTED
        
        /* Log that we set the bitmap and verify SQLAllocHandle is marked */
        trino_log("SQLGetFunctions: set ODBC3_ALL_FUNCTIONS bitmap, SQLAllocHandle=%d",
                  SQL_FUNC_EXISTS(array, SQL_API_SQLALLOCHANDLE));
        return SQL_SUCCESS;
    }

    switch (function_id) {
        case SQL_API_SQLALLOCHANDLE:
        case SQL_API_SQLFREEHANDLE:
        case SQL_API_SQLBINDCOL:
        case SQL_API_SQLBINDPARAMETER:
        case SQL_API_SQLCANCEL:
        case SQL_API_SQLCOLATTRIBUTE:
        case SQL_API_SQLDESCRIBECOL:
        case SQL_API_SQLCOLUMNS:
        case SQL_API_SQLCONNECT:
        case SQL_API_SQLDISCONNECT:
        case SQL_API_SQLDRIVERCONNECT:
        case SQL_API_SQLEXECDIRECT:
        case SQL_API_SQLEXECUTE:
        case SQL_API_SQLFETCH:
        case SQL_API_SQLFETCHSCROLL:
        case SQL_API_SQLGETDATA:
        case SQL_API_SQLGETDIAGFIELD:
        case SQL_API_SQLGETDIAGREC:
        case SQL_API_SQLGETINFO:
        case SQL_API_SQLGETFUNCTIONS:
        case SQL_API_SQLNUMRESULTCOLS:
        case SQL_API_SQLNUMPARAMS:
        case SQL_API_SQLPREPARE:
        case SQL_API_SQLROWCOUNT:
        case SQL_API_SQLMORERESULTS:
        case SQL_API_SQLGETCONNECTATTR:
        case SQL_API_SQLSETCONNECTATTR:
        case SQL_API_SQLGETSTMTATTR:
        case SQL_API_SQLSETSTMTATTR:
        case SQL_API_SQLGETENVATTR:
        case SQL_API_SQLSETENVATTR:
        case SQL_API_SQLTABLES: *supported = SQL_TRUE; break;

        default: *supported = SQL_FALSE; break;
    }

    return SQL_SUCCESS;
}
