#ifndef TRINO_ODBC_ERROR_H
#define TRINO_ODBC_ERROR_H

#include "trino_odbc.h"

/* Maximum number of diagnostic records per handle */
#define MAX_DIAG_RECORDS 10

/* Diagnostic record */
typedef struct {
    SQLCHAR sqlstate[6];
    SQLINTEGER native_error;
    SQLCHAR message_text[TRINO_MAX_MESSAGE_LEN];
    SQLSMALLINT message_len;
} trino_diag_record_t;

/* Diagnostics context — attached to every handle */
typedef struct {
    trino_diag_record_t records[MAX_DIAG_RECORDS];
    SQLUSMALLINT record_count;
    SQLUSMALLINT rec_number; /* current record pointer */
    SQLCHAR sqlstate[6];
    SQLINTEGER native_error;
} trino_diagnostics_t;

/* Error state codes */
#define TRINO_SQLSTATE_SUCCESS "00000"
#define TRINO_SQLSTATE_NO_DATA "01000"
#define TRINO_SQLSTATE_MORE_DATA "01004"
#define TRINO_SQLSTATE_INVALID_HANDLE "08003"
#define TRINO_SQLSTATE_CONN_NOTTRAN "08007"
#define TRINO_SQLSTATE_CONN_CLOSED "08003"
#define TRINO_SQLSTATE_INVALID_CONN "08003"
#define TRINO_SQLSTATE_INVALID_CURSOR "24000"
#define TRINO_SQLSTATE_INVALID_SQLSTATE "HY000"
#define TRINO_SQLSTATE_MEMORY_ISSUE "HY013"
#define TRINO_SQLSTATE_INVALID_STR_LEN "HY009"
#define TRINO_SQLSTATE_INVALID_HANDLE_VALUE "HY00C"
#define TRINO_SQLSTATE_OPTION_NOT_ALLOWED "HY011"
#define TRINO_SQLSTATE_OPTION_VALUE_OUT_OF_RANGE "HY012"
#define TRINO_SQLSTATE_REQUEST_FAILED "HY008"
#define TRINO_SQLSTATE_MEMORY_ALLOCATION "HY001"
#define TRINO_SQLSTATE_SOFT_FAILURE "HY009"
#define TRINO_SQLSTATE_PROTOCOL_ERROR "HY015"
#define TRINO_SQLSTATE_NETWORK_ERROR "HYT00"
#define TRINO_SQLSTATE_TIMEOUT "HYT01"
#define TRINO_SQLSTATE_WRITE_NOT_SUPPORTED "HY000"
#define TRINO_SQLSTATE_LOGIN_FAILED "28000"
#define TRINO_SQLSTATE_QUERY_FAILED "42000"
#define TRINO_SQLSTATE_SYNTAX_ERROR "42601"
#define TRINO_SQLSTATE_CATALOG_NOT_FOUND "42K01"
#define TRINO_SQLSTATE_SCHEMA_NOT_FOUND "42S02"
#define TRINO_SQLSTATE_TABLE_NOT_FOUND "42S02"
#define TRINO_SQLSTATE_COLUMN_NOT_FOUND "42703"
#define TRINO_SQLSTATE_TYPE_NOT_SUPPORTED "42804"
#define TRINO_SQLSTATE_PERMISSION_DENIED "42501"

/* Initialize diagnostics context */
void trino_diag_init(trino_diagnostics_t *diag);

/* Add a diagnostic record. sqlstate and message are C strings (the SQLSTATE
 * literals and message text are plain `char`). */
void trino_diag_add(trino_diagnostics_t *diag, const char *sqlstate,
                    SQLINTEGER native_error, const char *message);

/* Set the primary error (first record) */
void trino_diag_set_error(trino_diagnostics_t *diag, const char *sqlstate,
                          SQLINTEGER native_error, const char *message);

/* Clear all diagnostics */
void trino_diag_clear(trino_diagnostics_t *diag);

/* Format a Trino server error into SQLState + message */
void trino_diag_from_trino_error(trino_diagnostics_t *diag, const char *error_name,
                                 const char *error_message, const char *error_type);

#endif /* TRINO_ODBC_ERROR_H */
