/* SQLGetInfo and SQLGetFunctions implementation */

#include "trino_odbc/connection.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * SQLGetInfo - Returns information about the data source and driver
 * ======================================================================== */

SQLRETURN SQLGetInfo(SQLHDBC connection_handle, SQLUSMALLINT info_type,
                     SQLPOINTER char_attr, SQLINTEGER buffer_length,
                     SQLINTEGER *str_len)
{
    if (!connection_handle) return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn)) return SQL_INVALID_HANDLE;

    if (!char_attr) return SQL_INVALID_HANDLE;

    switch (info_type) {
        case SQL_INFO_ACCESSIBLE_TABLES:
            *(SQLBOOLEAN *)char_attr = SQL_FALSE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_INFO_ACCESSIBLE_PROCEDURES:
            *(SQLBOOLEAN *)char_attr = SQL_FALSE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_INFO_AGENT_NAME:
            strncpy((char *)char_attr, "Trino ODBC Driver", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_INFO_AGENT_VER:
            strncpy((char *)char_attr, TRINO_ODBC_VERSION_STR, (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_INFO_AUTO_COMMIT:
            *(SQLBOOLEAN *)char_attr = conn->autocommit ? SQL_TRUE : SQL_FALSE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_INFO_BATCH_SUPPORT:
            *(SQLUINTEGER *)char_attr = SQL_BATCH_ROWSET | SQL_BATCH_SINGULAR_MSG;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_INFO_CATALOG_LOCATION:
            *(SQLUINTEGER *)char_attr = SQL_CL_START;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_INFO_CATALOG_NAME:
            strncpy((char *)char_attr, "catalog", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_INFO_CATALOG_TERM:
            strncpy((char *)char_attr, "catalog", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_INFO_CATALOG_USAGE:
            *(SQLUINTEGER *)char_attr = SQL_CU_PROCEDURE_COLUMN | SQL_CU_TABLE_COLUMN;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_INFO_CLIENT_VERSION:
            strncpy((char *)char_attr, TRINO_ODBC_VERSION_STR, (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_INFO_CONCAT_NULL_BEHAVIOR:
            *(SQLUINTEGER *)char_attr = SQL_CSB_NULL;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_INFO_CURSOR_COMMIT_BEHAVIOR:
            *(SQLUINTEGER *)char_attr = SQL_CC_CLOSE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_INFO_CURSOR_ROLLBACK_BEHAVIOR:
            *(SQLUINTEGER *)char_attr = SQL_CC_CLOSE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_DATA_SOURCE_NAME:
            if (conn->server) {
                snprintf((char *)char_attr, (size_t)buffer_length, "%s:%d",
                         conn->server, conn->port);
            } else {
                strncpy((char *)char_attr, "Trino", (size_t)buffer_length - 1);
            }
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_DBMS_NAME:
            strncpy((char *)char_attr, "Trino", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_DBMS_VER:
            /* We don't know the server version until we connect */
            strncpy((char *)char_attr, "357.1", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_DEFAULT_TXN_ISOLATION:
            *(SQLUINTEGER *)char_attr = SQL_TXN_READ_COMMITTED;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_DRIVER_HDESC:
            *(SQLBOOLEAN *)char_attr = SQL_TRUE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_DRIVER_HLIB:
            *(SQLBOOLEAN *)char_attr = SQL_TRUE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_DRIVER_HSTMT:
            *(SQLBOOLEAN *)char_attr = SQL_TRUE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_DRIVER_HENV:
            *(SQLBOOLEAN *)char_attr = SQL_TRUE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_DRIVER_NAME:
            strncpy((char *)char_attr, "TrinoODBC", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_DRIVER_VER:
            strncpy((char *)char_attr, TRINO_ODBC_VERSION_STR, (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_FILE_USAGE:
            *(SQLUINTEGER *)char_attr = 0;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_GETDATA_EXTENSIONS:
            *(SQLUINTEGER *)char_attr = SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_IDENTIFIER_QUOTE_CHAR:
            strncpy((char *)char_attr, "\"", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = 1;
            break;

        case SQL_INFO_SCHEMA_SQL_CONFORMANCE:
            *(SQLUINTEGER *)char_attr = SQL_IC_NONE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_KEYWORDS:
            strncpy((char *)char_attr,
                    "ADD,ALL,ALTER,AND,ANY,AS,ASC,ASSERT_TRUE,AT,AUTHORIZATION,"
                    "BETWEEN,BOTH,BY,CASE,CASCADE,CAST,CHECK,COALESCE,COLLATE,"
                    "COLUMN,COMMIT,COMPUTE,CONFLICT,CONNECT,CONNECT_BY_ROOT,COPY,"
                    "CREATE,CROSS,CUBE,CURRENT,CURRENT_DATE,CURRENT_TIME,"
                    "CURRENT_TIMESTAMP,CURRENT_USER,CYCLE,DATA,DATE,DAY,DEALLOCATE,"
                    "DEC,DECIMAL,DECLARE,DEFAULT,DELETE,DELTA,DESC,DETERMINISTIC,"
                    "DICTIONARY,DISTINCT,DML,DO,DOUBLE,DROP,ELSE,END,EPOLL,EXCEPT,"
                    "EXECUTE,EXISTS,EXPansion,EXPLAIN,EXTENDS,EXTERNAL,EXTRACT,False,"
                    "FETCH,FIRST,FLOAT,FOR,Force,FOREIGN,FREEZE,FROM,FULL,FUNCTION,"
                    "FUNCTIONS,FUTURE,GLOB,GRANT,GROUP,GROUPING,GROUPS,HAVING,HOUR,"
                    "ILIKE,IN,INITIALLY,INNER,INOUT,INPUT,INSENSITIVE,INSERT,INT,"
                    "INTEGER,INTERSECT,INTERVAL,INTO,IS,ISODOW,ISOLATION,ISOWEEK,"
                    "ISOWEPOCH,JOIN,LATERAL,LEADING,LEFT,LIKE,LIMIT,LN,LOAD,LOCALTIME,"
                    "LOCALTIMESTAMP,LOCK,MAXVALUE,MERGE,METADATA,MINUS,MINUTE,MINVALUE,"
                    "ML,MOD,MODULE,MONTH,NATURAL,NCHAR,NEXT,NEXTVAL,NO,NONE,NOT,NOTHING,"
                    "NTILE,NULL,NULLIF,NUMERIC,OF,OFF,OFFSET,OID,OLD,ON,ONLY,OPCACHE,"
                    "OPERATOR,OPTIONS,OR,ORDER,OUT,OUTER,OVER,OVERLAPS,PARAMETER,PARTITION,"
                    "PERCENT,PERCENTILE,PIVOT,PLACING,PORTION,PRECEDING,PRIMARY,"
                    "PROCEDURE,RANGE,READ,REAL,RECURSIVE,REF,REFERENCES,REINDEX,RELEASE,"
                    "RENAME,REPEATABLE,REPLACE,RESTRICT,RETAIN,RETURNING,REVOKE,RIGHT,"
                    "ROLLBACK,ROLLUP,ROW,ROWS,RUNNING,SCALAR,SCHEMA,SELECT,SESSION_USER,"
                    "SET,SETOF,SETS,SHOW,SIMILAR,SOME,SQL,START,STATS,STRING,SYSTEM,"
                    "TABLE,TABLESAMPLE,THEN,TIES,TIME,TIMESTAMP,TO,TRAILING,TRANSACTION,"
                    "TREAT,TRIGGER,True,TRUNCATE,TYPE,UESCAPE,UNBOUNDED,UNION,UNIQUE,"
                    "UNKNOWN,UNPIVOT,UPDATE,USER,VALUES,VARCAl,VERBOSE,VERSIONNING,"
                    "VIEW,VOLATILE,WARN,WATERMARK,WINDOW,WITH,WITHOUT,WORK,WRITE,XML,"
                    "YEAR,YEAR_MONTH,ZONE",
                    (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_LIKE_ESCAPE_CLAUSE:
            *(SQLBOOLEAN *)char_attr = SQL_TRUE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_MAX_CATALOG_NAME_LEN:
            *(SQLUINTEGER *)char_attr = SQL_MAX_CATALOG_NAME_LEN;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_COLUMN_NAME_LEN:
            *(SQLUINTEGER *)char_attr = SQL_MAX_IDENTIFIER_LEN;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_COLUMNS_IN_GROUP_BY:
            *(SQLUINTEGER *)char_attr = SQL_IU_MAX;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_COLUMNS_IN_INDEX:
            *(SQLUINTEGER *)char_attr = SQL_IU_MAX;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_COLUMNS_IN_ORDER_BY:
            *(SQLUINTEGER *)char_attr = SQL_IU_MAX;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_COLUMNS_IN_SELECT:
            *(SQLUINTEGER *)char_attr = SQL_IU_MAX;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_COLUMNS_IN_TABLE:
            *(SQLUINTEGER *)char_attr = SQL_IU_MAX;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_CONNECTIONS:
            *(SQLUINTEGER *)char_attr = SQL_MAX_DRIVER_CONNECTIONS;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_CURSOR_NAME_LEN:
            *(SQLUINTEGER *)char_attr = SQL_MAX_IDENTIFIER_LEN;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_IDENTIFIER_LEN:
            *(SQLUINTEGER *)char_attr = SQL_MAX_IDENTIFIER_LEN;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_INDEX_SIZE:
            *(SQLUINTEGER *)char_attr = SQL_IU_MAX;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_SCHEMA_NAME_LEN:
            *(SQLUINTEGER *)char_attr = SQL_MAX_SCHEMA_NAME_LEN;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_TABLE_NAME_LEN:
            *(SQLUINTEGER *)char_attr = SQL_MAX_TABLE_NAME_LEN;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_MAX_TABLES_IN_SELECT:
            *(SQLUINTEGER *)char_attr = SQL_IU_MAX;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_NEED_LONG_DATA_LEN:
            *(SQLBOOLEAN *)char_attr = SQL_FALSE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_NON_NULLABLE_COLUMNS:
            *(SQLUINTEGER *)char_attr = SQL_NNC_NON_NULL;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_NULL_COLLATION:
            *(SQLUINTEGER *)char_attr = SQL_NC_START;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ODBC_API_CONFORMANCE:
            *(SQLUINTEGER *)char_attr = SQL_OAC_LEVEL_1;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ODBC_SQL_CONFORMANCE:
            *(SQLUINTEGER *)char_attr = SQL_OSC_CORE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ODBC_VER:
            strncpy((char *)char_attr, "03.80", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_OJ_CAPABILITIES:
            *(SQLUINTEGER *)char_attr =
                SQL_OJ_LEFT | SQL_OJ_RIGHT | SQL_OJ_FULL |
                SQL_OJ_NESTED | SQL_OJ_NOT_DEFERRABLE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ORDER_BY_COLUMNS_IN_SELECT:
            *(SQLBOOLEAN *)char_attr = SQL_TRUE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_PARAM_ARRAY_ROW_SETS:
            *(SQLUINTEGER *)char_attr = 0;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_PARAM_ARRAY_ROW_SETS_TIMEOUT:
            *(SQLUINTEGER *)char_attr = 0;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_PARAM_ARRAY_UPDATE_THRESHOLD:
            *(SQLUINTEGER *)char_attr = 0;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_PROCEDURES:
            *(SQLBOOLEAN *)char_attr = SQL_FALSE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLBOOLEAN);
            break;

        case SQL_QUOTED_IDENTIFIER_CASE:
            *(SQLUINTEGER *)char_attr = SQL_IC_SENSITIVE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ROW_UPDATABILITY:
            *(SQLUINTEGER *)char_attr = SQLRU_NEVER_UPDATABLE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_SCHEMA_TERM:
            strncpy((char *)char_attr, "schema", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_SERVER_NAME:
            if (conn->server) {
                strncpy((char *)char_attr, conn->server, (size_t)buffer_length - 1);
                ((char *)char_attr)[buffer_length - 1] = '\0';
                if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            } else {
                if (str_len) *str_len = 0;
            }
            break;

        case SQL_SPECIAL_CHARACTERS:
            strncpy((char *)char_attr, "_", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_STATIC_CURSOR_ATTRIBUTES1:
            *(SQLUINTEGER *)char_attr = SQL_SCA_SENSITIVE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_STATIC_CURSOR_ATTRIBUTES2:
            *(SQLUINTEGER *)char_attr = 0;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_SYSTEM_FUNCTIONS:
            *(SQLUINTEGER *)char_attr = SQL_SF_LOCATE_U;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_TABLE_TYPES:
            strncpy((char *)char_attr, "TABLE;VIEW", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        case SQL_TXN_ISOLATION_OPTION:
            *(SQLUINTEGER *)char_attr =
                SQL_TXN_READ_COMMITTED | SQL_TXN_REPEATABLE_READ |
                SQL_TXN_SERIALIZABLE;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_UNICODE_CHAR_BASE_TYPE:
            *(SQLUINTEGER *)char_attr = SQL_WCHAR;
            if (str_len) *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_USER_NAME:
            if (conn->user) {
                strncpy((char *)char_attr, conn->user, (size_t)buffer_length - 1);
                ((char *)char_attr)[buffer_length - 1] = '\0';
                if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            } else {
                if (str_len) *str_len = 0;
            }
            break;

        case SQL_XOPEN_CLI_YEAR:
            strncpy((char *)char_attr, "2019", (size_t)buffer_length - 1);
            ((char *)char_attr)[buffer_length - 1] = '\0';
            if (str_len) *str_len = (SQLINTEGER)strlen((char *)char_attr);
            break;

        default:
            /* Unknown info type - return empty */
            if (str_len) *str_len = 0;
            break;
    }

    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLGetFunctions - Returns a bitmask indicating which functions are supported
 * ======================================================================== */

SQLRETURN SQLGetFunctions(SQLHDBC connection_handle, SQLUSMALLINT function,
                          SQLUSHORT *supported)
{
    if (!connection_handle || !supported) return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn)) return SQL_INVALID_HANDLE;

    /* Supported functions bitmask */
    *supported =
        SQL_FN_SQL_ALLOCHANDLE /* SQLAllocHandle */ |
        SQL_FN_SQL_ALLOCstmt   /* SQLAllocStmt */ |
        SQL_FN_SQL_BINDCOL     /* SQLBindCol */ |
        SQL_FN_SQL_BINDPARAM   /* SQLBindParameter */ |
        SQL_FN_SQL_COLATTRIBUTE /* SQLColAttribute */ |
        SQL_FN_SQL_COLUMNPRIVILEGES /* SQLColumnPrivileges */ |
        SQL_FN_SQL_COLUMNS     /* SQLColumns */ |
        SQL_FN_SQL_CONNECT     /* SQLConnect */ |
        SQL_FN_SQL_DATASOURCES /* SQLDataSources */ |
        SQL_FN_SQL_DESCRIBE    /* SQLDescribeCol */ |
        SQL_FN_SQL_DRIVERCONNECT /* SQLDriverConnect */ |
        SQL_FN_SQL_DRIVERS     /* SQLDrivers */ |
        SQL_FN_SQL_EXEC_DIRECT |
        SQL_FN_SQL_FETCH       /* SQLFetch */ |
        SQL_FN_SQL_GETDATA     /* SQLGetData */ |
        SQL_FN_SQL_GETDIAGFIELD /* SQLGetDiagField */ |
        SQL_FN_SQL_GETDIAGREC  /* SQLGetDiagRec */ |
        SQL_FN_SQL_GETINFO     /* SQLGetInfo */ |
        SQL_FN_SQL_NUMRESULTCOLS /* SQLNumResultCols */ |
        SQL_FN_SQL_NUMPARAMS   /* SQLNumParams */ |
        SQL_FN_SQL_PARAMDATA   /* SQLParamData */ |
        SQL_FN_SQL_PREPARE     /* SQLPrepare */ |
        SQL_FN_SQL_PRIMARYKEYS /* SQLPrimaryKeys */ |
        SQL_FN_SQL_PROCCOLUMNS /* SQLProcedureColumns */ |
        SQL_FN_SQL_PROCEDURES  /* SQLProcedures */ |
        SQL_FN_SQL_ROWCOUNT    /* SQLRowCount */ |
        SQL_FN_SQL_SETPos      /* SQLSetPos */ |
        SQL_FN_SQL_SPECIALCOLUMNS /* SQLSpecialColumns */ |
        SQL_FN_SQL_STATISTICS  /* SQLStatistics */ |
        SQL_FN_SQL_TABLES      /* SQLTables */ |
        SQL_FN_SQL_TABLEPRIVILEGES /* SQLTablePrivileges */;

    return SQL_SUCCESS;
}
