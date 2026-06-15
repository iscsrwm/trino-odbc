#ifndef TRINO_ODBC_H
#define TRINO_ODBC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * ODBC Return Codes
 * ======================================================================== */

typedef uint16_t SQLRETURN;
#define SQL_SUCCESS           0
#define SQL_SUCCESS_WITH_INFO 1
#define SQL_ERROR            100
#define SQL_INVALID_HANDLE   101
#define SQL_NEED_DATA        99
#define SQL_NO_DATA          100

/* ========================================================================
 * ODBC Handle Types
 * ======================================================================== */

typedef uint16_t SQLUSMALLINT;
typedef uint32_t SQLUWORD;
typedef uint64_t SQLULEN;
typedef int64_t  SQLLEN;
typedef int64_t  SQLROWID;
typedef SQLLEN   SQLBUFFER_LENGTH;
typedef uint32_t SQLULONG;
typedef int32_t  SQLLONG;
typedef int16_t  SQLSMALLINT;
typedef int16_t SQLSWORD;

/* Opaque handle */
typedef void *SQLHANDLE;

#define SQL_NULL_HANDLE 0

#define SQL_HANDLE_ENV      1
#define SQL_HANDLE_DBC      2
#define SQL_HANDLE_STMT     3
#define SQL_HANDLE_DESC     4

/* Opaque handle pointers */
typedef void *SQLHENV;
typedef void *SQLHDBC;
typedef void *SQLHSTMT;
typedef void *SQLHDESC;

/* ========================================================================
 * Character Types
 * ======================================================================== */

typedef char SQLCHAR;
typedef void *SQLPOINTER;

/* ========================================================================
 * Numeric Types
 * ======================================================================== */
/* Numeric types */
typedef int8_t   SQLBYTE;
typedef uint8_t  SQLUBYTE;
typedef int16_t  SQLSHORT;
typedef uint16_t SQLUSHORT;
typedef int32_t  SQLINTEGER;
typedef uint32_t SQLUINTEGER;
typedef int64_t  SQLBIGINT;
typedef uint64_t SQLUBIGINT;
typedef float    SQLFLOAT;
typedef double   SQLDOUBLE;
/* ========================================================================
 * Special Values
 * ======================================================================== */

#define SQL_NULL_DATA       (-2)
#define SQL_NTS             (-3)        /* Null-terminated string */
#define SQL_DATA_AT_EXEC    (-9)
#define SQL_DEFAULT_PARAM   (-6)

/* ========================================================================
 * Parameter Types
 * ======================================================================== */

#define SQL_PARAM_INPUT     1
#define SQL_PARAM_OUTPUT    2
#define SQL_PARAM_INPUT_OUTPUT 3

/* ========================================================================
 * C Data Types (SQL_C_*)
 * ======================================================================== */

#define SQL_C_TINYINT       (-6)
#define SQL_C_STINYINT      (-55)
#define SQL_C_UTINYINT      (-56)
#define SQL_C_SHORT         (-2)
#define SQL_C_USHORT        (-14)
#define SQL_C_LONG          (-3)
#define SQL_C_ULONG         (-13)
#define SQL_C_INT           (-6)
#define SQL_C_BIGINT        (-8)
#define SQL_C_UBIGINT       (-38)
#define SQL_C_FLOAT         (-4)
#define SQL_C_DOUBLE        (-5)
#define SQL_C_BIT           (-11)
#define SQL_C_CHAR          1
#define SQL_C_BINARY        (-2)
#define SQL_C_TYPE_DATE     (-8)
#define SQL_C_TYPE_TIME     (-9)
#define SQL_C_TYPE_TIMESTAMP (-10)
#define SQL_C_INTERVAL_YEAR         (-112)
#define SQL_C_INTERVAL_MONTH        (-113)
#define SQL_C_INTERVAL_DAY          (-114)
#define SQL_C_INTERVAL_HOUR         (-115)
#define SQL_C_INTERVAL_MINUTE       (-116)
#define SQL_C_INTERVAL_SECOND       (-117)
#define SQL_C_INTERVAL_YEAR_TO_MONTH     (-118)
#define SQL_C_INTERVAL_DAY_TO_HOUR       (-119)
#define SQL_C_INTERVAL_DAY_TO_MINUTE     (-120)
#define SQL_C_INTERVAL_DAY_TO_SECOND     (-121)
#define SQL_C_INTERVAL_HOUR_TO_MINUTE    (-122)
#define SQL_C_INTERVAL_HOUR_TO_SECOND    (-123)
#define SQL_C_INTERVAL_MINUTE_TO_SECOND  (-124)

/* ========================================================================
 * SQL Data Types (SQL_*)
 * ======================================================================== */

#define SQL_CHAR          1
#define SQL_VARCHAR      12
#define SQL_LONGVARCHAR  (-1)
#define SQL_DECIMAL      3
#define SQL_NUMERIC      2
#define SQL_SMALLINT     5
#define SQL_INTEGER      4
#define SQL_BIGINT      (-5)
#define SQL_REAL         7
#define SQL_FLOAT        6
#define SQL_DOUBLE       8
#define SQL_BIT          (-7)
#define SQL_TINYINT      (-6)
#define SQL_BINARY       (-2)
#define SQL_VARBINARY    (-3)
#define SQL_LONGVARBINARY (-4)
#define SQL_DATE         91
#define SQL_TIME         92
#define SQL_TIMESTAMP    93
#define SQL_TYPE_DATE    (-8)
#define SQL_TYPE_TIME    (-9)
#define SQL_TYPE_TIMESTAMP (-10)
#define SQL_INTERVAL_YEAR             (-103)
#define SQL_INTERVAL_MONTH            (-104)
#define SQL_INTERVAL_DAY              (-105)
#define SQL_INTERVAL_HOUR             (-106)
#define SQL_INTERVAL_MINUTE           (-107)
#define SQL_INTERVAL_SECOND           (-108)
#define SQL_INTERVAL_YEAR_TO_MONTH    (-109)
#define SQL_INTERVAL_DAY_TO_HOUR      (-110)
#define SQL_INTERVAL_DAY_TO_MINUTE    (-111)
#define SQL_INTERVAL_DAY_TO_SECOND    (-113)
#define SQL_INTERVAL_HOUR_TO_MINUTE   (-114)
#define SQL_INTERVAL_HOUR_TO_SECOND   (-115)
#define SQL_INTERVAL_MINUTE_TO_SECOND (-116)
#define SQL_GUID           (-15)

/* ========================================================================
 * Buffer Sizes
 * ======================================================================== */

#define SQL_MAX_BUFFER_LENGTH    1024
#define SQL_MAX_IDENTIFIER_LEN   128
#define SQL_MAX_MESSAGE_LEN      512
#define SQL_MAX_DATETIME_CSRCP   26
#define SQL_MAX_DRIVER_CONN_STR_LEN  1024

/* ========================================================================
 * Environment Attributes (SQL_ATTR_*)
 * ======================================================================== */

#define SQL_ATTR_ODBC_VERSION             200
#define SQL_ATTR_CONNECTION_POOLING       201
#define SQL_ATTR_CP_MATCH                 202
#define SQL_ATTR_ACCESS_MODE              105
#define SQL_ATTR_AUTOCOMMIT               102
#define SQL_ATTR_LOGIN_TIMEOUT            112
#define SQL_ATTR_QUiet_MODE               12501

#define SQL_OV_ODBC2                      2
#define SQL_OV_ODBC3                      3
#define SQL_OV_ODBC3_80                   380

#define SQL_CP_OFF                        0
#define SQL_CP_ONE_PER_HENV               1
#define SQL_CP_DEFAULT                    2

#define SQL_CP_STRICT_MATCH               0
#define SQL_CP_RELAXED_ISOLATED           1
#define SQL_CP_RELAXED_ALL_CONNECTIONS    2
#define SQL_CP_RELAXED_ALL_PROMOTED       3

/* ========================================================================
 * Connection Attributes
 * ======================================================================== */

#define SQL_ACCESS_MODE                   105
#define SQL_MODE_READ_ONLY                1
#define SQL_MODE_READ_WRITE               2
#define SQL_AUTOCOMMIT                    102
#define SQL_AUTOCOMMIT_ON                 1
#define SQL_AUTOCOMMIT_OFF                0
#define SQL_CURRENT_CATALOG               103
#define SQL_LOGIN_TIMEOUT                 112
#define SQL_PACKET_SIZE                   114
#define SQL_TRANSLATE_OPTION              125
#define SQL_TRANSLATE_LIB                 126

#define SQL_ATTR_ACCESS_MODE              105
#define SQL_ATTR_AUTOCOMMIT               102
#define SQL_ATTR_CURRENT_CATALOG          103
#define SQL_ATTR_LOGIN_TIMEOUT            112
#define SQL_ATTR_PACKET_SIZE              114
#define SQL_ATTR_TRANSLATE_OPTION         125
#define SQL_ATTR_CONN_STRING              1268
#define SQL_ATTR_CONNECTION_TIMEOUT       12500
#define SQL_ATTR_QUIET_MODE               12501
#define SQL_ATTR_ANsi_MODE                12503
#define SQL_ATTR_ODBC_CURSORS             12504
#define SQL_ATTR_PRETRIM_FIELDS           12512

#define SQL_ANSI_OFF                      0
#define SQL_ANSI_ON                       1

#define SQL_ODBC_CURSORS_OFF              0
#define SQL_ODBC_CURSORS_ON               1

/* ========================================================================
 * Statement Attributes
 * ======================================================================== */

#define SQL_ATTR_APP_PARAM_DESC           12930
#define SQL_ATTR_APP_ROW_DESC             12931
#define SQL_ATTR_IMP_PARAM_DESC           12932
#define SQL_ATTR_IMP_ROW_DESC             12933
#define SQL_ATTR_CONCURRENCY              1026
#define SQL_ATTR_CURSOR_TYPE              1027
#define SQL_ATTR_CURSOR_SCROLL_ATTR       12101
#define SQL_ATTR_ENABLE_AUTO_IPD          12118
#define SQL_ATTR_FETCH_BOOKMARK_PTR       12102
#define SQL_ATTR_KEYSET_SIZE              12103
#define SQL_ATTR_MAX_LENGTH               12104
#define SQL_ATTR_MAX_ROWS                 12105
#define SQL_ATTR_PARAMS                   12106
#define SQL_ATTR_PARAM_BIND_OFFSET_PTR    12107
#define SQL_ATTR_PARAM_BIND_TYPE          12108
#define SQL_ATTR_PARAM_OPERATION_PTR      12109
#define SQL_ATTR_PARAM_STATUS_PTR         12110
#define SQL_ATTR_PARAMS_BIND_TYPE_OFFEST_PTR 12111
#define SQL_ATTR_PARAMSET_SIZE            12112
#define SQL_ATTR_QUERY_TIMEOUT            1025
#define SQL_ATTR_RETRIEVE_DATA            12113
#define SQL_ATTR_ROW_ARRAY_SIZE           12114
#define SQL_ATTR_ROW_BIND_OFFSETS         12115
#define SQL_ATTR_ROW_BIND_TYPE            12116
#define SQL_ATTR_ROW_COUNT                12117
#define SQL_ATTR_ROW_NUMBER               12120
#define SQL_ATTR_ROW_STATUS_PTR           12121
#define SQL_ATTR_ROWS_FETCHED_PTR         12122

#define SQL_CONCUR_READ_ONLY              1
#define SQL_CONCUR_LOCK                   2
#define SQL_CONCUR_ROWVER                 3
#define SQL_CONCUR_VALUES                 4

#define SQL_CURSOR_TYPE_UNKNOWN           0
#define SQL_CURSOR_FORWARD_ONLY           1
#define SQL_CURSOR_KEYSET_DRIVEN          2
#define SQL_CURSOR_DYNAMIC                3
#define SQL_CURSOR_STATIC                 4

#define SQL_SCROLL_LOCKTYPE_CHANGE        2
#define SQL_SCROLL_LOCKTYPE_NONE          0
#define SQL_SCROLL_LOCKTYPE_OPTIMISTIC    1

#define SQL_FETCH_FORWARD                 1
#define SQL_FETCH_BACKWARD                (-1)
#define SQL_FETCH_FIRST                   2
#define SQL_FETCH_LAST                    (-2)
#define SQL_FETCH_NEXT                    3
#define SQL_FETCH_PRIOR                   (-3)
#define SQL_FETCH_ABSOLUTE                4
#define SQL_FETCH_RELATIVE                (-4)

/* ========================================================================
 * Descriptor Fields
 * ======================================================================== */

#define SQL_DESC_ALLOC_TYPE               1
#define SQL_DESC_ARRAY_SIZE               4
#define SQL_DESC_ARRAY_STATUS_PTR         5
#define SQL_DESC_ATTR_SIZE_OF_ARRAY       12996
#define SQL_DESC_AUTO_UNIQUE_VALUE        12997
#define SQL_DESC_BASE_COLUMN_NAME         12998
#define SQL_DESC_BASE_TABLE_NAME          12999
#define SQL_DESC_BYTES                   13000
#define SQL_DESC_CATALOG_NAME             13001
#define SQL_DESC_DATETIME_INTERVAL_CODE   13002
#define SQL_DESC_DATETIME_INTERVAL_PRECISION 13003
#define SQL_DESC_DEFAULT_VALUE            13004
#define SQL_DESC_DISPLAY_SIZE             13005
#define SQL_DESC_FIXED_PREC_SCALE         13006
#define SQL_DESC_FILE_NAME                13007
#define SQL_DESC_LITERAL_PREFIX           13008
#define SQL_DESC_LITERAL_SUFFIX           13009
#define SQL_DESC_LABEL                    13010
#define SQL_DESC_LENGTH                   4
#define SQL_DESC_LOCAL_TYPE_NAME          13011
#define SQL_DESC_NAME                     2
#define SQL_DESC_NULLABLE                 13012
#define SQL_DESC_NUM_PREC_RADIX           13013
#define SQL_DESC_OCTET_LENGTH_PTR         13014
#define SQL_DESC_PRECISION                6
#define SQL_DESC_SCALE                    7
#define SQL_DESC_SCHEMA_NAME              13015
#define SQL_DESC_SEARCHABLE               13016
#define SQL_DESC_UNNAMED                  13017
#define SQL_DESC_TYPE                     1
#define SQL_DESC_TYPE_NAME                3
#define SQL_DESC_UNSIGNED                 13018
#define SQL_DESC_UPDATABLE                13019

#define SQL_DESC_COUNT                    0
#define SQL_DESC_BIND_OFFSET              12991
#define SQL_DESC_BIND_TYPE                12992
#define SQL_DESC_BIND_OFFSET_PTR          12995
#define SQL_DESC_DATA_PTR                 5
#define SQL_DESC_BUFFER_LENGTH            14
#define SQL_DESC_INDICATOR_PTR            15
#define SQL_DESC_OCTET_LENGTH             16

#define SQL_ALLOC_DRIVER                  0
#define SQL_ALLOC_SYSTEM                  1

/* ========================================================================
 * SQLGetInfo Types
 * ======================================================================== */

#define SQL_ACTIVE_ENVIRONMENTS           0
#define SQL_AGGR_SUPPORT_LEVEL            127
#define SQL_AGGREGATE_FUNCTIONS           195
#define SQL_ALLOCATE_CD_SQLSTATE          128
#define SQL_DATA_SOURCE_NAME              17
#define SQL_DATA_SOURCE_READ_ONLY         25
#define SQL_DEFAULT_TXN_ISOLATION         130
#define SQL_DRIVER_HDBC                   14
#define SQL_DRIVER_HDESC                  15
#define SQL_DRIVER_HENV                   16
#define SQL_DRIVER_HLIB                   12
#define SQL_DRIVER_HSTMT                  13
#define SQL_DRIVER_NAME                   1603
#define SQL_DRIVER_VER                    1701
#define SQL_DRIVER_ODBC_VER               1700
#define SQL_FILE_USAGE                    129
#define SQL_FILE_FILE                   131
#define SQL_FILE_FILE_SQLSTATE            132
#define SQL_FILE_LOCATION                 133
#define SQL_FILE_NAME                    134
#define SQL_GETDATA_EXTENSIONS            127
#define SQL_IDENTIFIER_QUOTE_CHAR         4
#define SQL_MAX_COLUMN_NAME_LEN           39
#define SQL_MAX_COLUMNS_IN_GROUP_BY       40
#define SQL_MAX_COLUMNS_IN_INDEX          41
#define SQL_MAX_COLUMNS_IN_ORDER_BY       42
#define SQL_MAX_COLUMNS_IN_SELECT         43
#define SQL_MAX_COLUMNS_IN_TABLE          44
#define SQL_MAX_CURSOR_NAME_LEN           45
#define SQL_MAX_DRIVER_CONNECTIONS        46
#define SQL_INFO_MAX_IDENTIFIER_LEN       47
#define SQL_MAX_INDEX_SIZE                48
#define SQL_MAX_SCHEMA_NAME_LEN           50
#define SQL_MAX_CATALOG_NAME_LEN          49
#define SQL_MAX_TABLE_NAME_LEN            51
#define SQL_MAX_TABLES_IN_SELECT          52
#define SQL_MAX_USER_NAME_LEN             53
#define SQL_ODBC_API_CONFORMANCE          110
#define SQL_ODBC_VER                      1602
#define SQL_PARAM_ARRAY_ROW_COUNTS        20001
#define SQL_PARAM_ARRAY_STRUCTS           20002
#define SQL_QUERY_TIMEOUT                 23
#define SQL_TXN_ISOLATION_OPTION          26
#define SQL_TXN_CAPABLE                   27
#define SQL_USER_NAME                     30

#define SQL_TXN_CAPABLE_NOT               0
#define SQL_TXN_CAPABLE_READ              1
#define SQL_TXN_CAPABLE_WRITE             2

#define SQL_TXN_READ_UNCOMMITTED          1
#define SQL_TXN_READ_COMMITTED            2
#define SQL_TXN_REPEATABLE_READ           3
#define SQL_TXN_SERIALIZABLE              4

/* ========================================================================
 * Column Searchability
 * ======================================================================== */

#define SQL_NO_USAGE                      0
#define SQL_USAGE_LIKE                   1
#define SQL_USAGE_BETWEEN                 2
#define SQL_USAGE_COMPARE                4
#define SQL_USAGE_GROUP_BY               8
#define SQL_USAGE_HAVING                 16
#define SQL_USAGE_ORDER_BY               32
#define SQL_USAGE_UNRESTRICTED           63

/* Nullability */
#define SQL_NON_NULLABLE                  0
#define SQL_NULLABLE                      1
#define SQL_NULLABLE_UNKNOWN              2

/* Updatability */
#define SQL_ATTR_READ_ONLY                0
#define SQL_ATTR_UPDATABLE                1
#define SQL_ATTR_UPDATABLE_UNKNOWN        2
#define SQL_ATTR_MOD_WRITE                3
#define SQL_ATTR_MOD_DELETE               4

/* ========================================================================
 * SQLFunctions / SQLGetFunctions
 * ======================================================================== */

#define SQL_API_SQLALLOCHANDLE            1
#define SQL_API_SQLALLOCCONNECT           2
#define SQL_API_SQLALLOCENV               3
#define SQL_API_SQLALLOCSTMT              4
#define SQL_API_SQLBINDCOL                5
#define SQL_API_SQLBINDPARAM              6
#define SQL_API_SQLCANCEL                 7
#define SQL_API_SQLCOLATTRIBUTE           8
#define SQL_API_SQLCOLATTRIBUTEW          9
#define SQL_API_SQLCOLUMNS                10
#define SQL_API_SQLCOLUMNSW               11
#define SQL_API_SQLCOLUMNPRIVILEGES       12
#define SQL_API_SQLCOLUMNPRIVILEGESW      13
#define SQL_API_SQLCONNECT                14
#define SQL_API_SQLCONNECTW               15
#define SQL_API_SQLCOPYDESC               16
#define SQL_API_SQLDATASOURCES            17
#define SQL_API_SQLDATASOURCESW           18
#define SQL_API_SQLDESCRIBE               19
#define SQL_API_SQLDRIVERS                20
#define SQL_API_SQLDRIVERSW               21
#define SQL_API_SQLDRIVERCONNECT          22
#define SQL_API_SQLDRIVERCONNECTW         23
#define SQL_API_SQLEXECUTE               24
#define SQL_API_SQLEXECDIRECT            25
#define SQL_API_SQLEXECDIRECTW           26
#define SQL_API_SQLFETCH                 27
#define SQL_API_SQLFETCHSCROLL           28
#define SQL_API_SQLFREEHANDLE            29
#define SQL_API_SQLFREECONNECT           30
#define SQL_API_SQLFREEENV               31
#define SQL_API_SQLFREESTMT              32
#define SQL_API_SQLGETCONNECTATTR        33
#define SQL_API_SQLGETCONNECTOPTION      34
#define SQL_API_SQLGETDATA               35
#define SQL_API_SQLGETDIAgFIELD          36
#define SQL_API_SQLGETDIAGREC            37
#define SQL_API_SQLGETENVATTR            38
#define SQL_API_SQLGETFUNCTIONS          39
#define SQL_API_SQLGETINFO               40
#define SQL_API_SQLGETINFOW              41
#define SQL_API_SQLGETTYPEINFO           42
#define SQL_API_SQLGETTYPEINFOW          43
#define SQL_API_SQLMORERESULTS           44
#define SQL_API_SQLNUMPARAMS             45
#define SQL_API_SQLNUMRESULTCOLS         46
#define SQL_API_SQLPARAMDATA             47
#define SQL_API_SQLPREPARE               48
#define SQL_API_SQLPREPAREW              49
#define SQL_API_SQLPUTDATA               50
#define SQL_API_SQLROWCOUNT              51
#define SQL_API_SQLSETCONNECTATTR        52
#define SQL_API_SQLSETCONNECTOPTION      53
#define SQL_API_SQLSETENVATTR            54
#define SQL_API_SQLSETPARAM              55
#define SQL_API_SQLSETSCROLLOPTIONS      56
#define SQL_API_SQLSETSTMTATTR           57
#define SQL_API_SQLSETSTMTOPTION         58
#define SQL_API_SQLSPECIALCOLUMNS        59
#define SQL_API_SQLSTATMENTATTR          60
#define SQL_API_SQLSTATMENTOPTION        61
#define SQL_API_SQLTABLES                62
#define SQL_API_SQLTABLESW               63
#define SQL_API_SQLTRANSACT              64

#define SQL_SUPPORTS_OPTIONAL            0
#define SQL_SUPPORTS_REQUIRED            1
#define SQL_SUPPORTS_UNUSED              2
#define SQL_SUPPORTS_UNSUPPORTED         3

/* ========================================================================
 * Statistics types
 * ======================================================================== */

#define SQL_STATS_HYBRID                0
#define SQL_STATS_INDEX                 1
#define SQL_STATS_RESERVED              2
#define SQL_STATS_TABLE                 3

#define SQL_UNIQUE                      1
#define SQL_BEST                        2
#define SQL_ALL                         3

/* ========================================================================
 * Trino-specific extensions
 * ======================================================================== */

#define SQL_TRINO_QUERY_ID             12001
#define SQL_TRINO_QUERY_STATE          12002
#define SQL_TRINO_QUERY_ELAPSED_TIME   12003
#define SQL_TRINO_QUERY_ROWS_PROCESSED 12004
#define SQL_TRINO_QUERY_BYTES_PROCESSED 12005

#ifdef __cplusplus
}
#endif

#endif /* TRINO_ODBC_H */
