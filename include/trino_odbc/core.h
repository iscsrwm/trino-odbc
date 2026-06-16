#ifndef TRINO_ODBC_CORE_H
#define TRINO_ODBC_CORE_H

#include "trino_odbc.h"
#include "trino_odbc/error.h"
#include <pthread.h>

/* ========================================================================
 * Handle types
 * ======================================================================== */

typedef enum {
    TRINO_HANDLE_ENV,
    TRINO_HANDLE_DBC,
    TRINO_HANDLE_STMT,
    TRINO_HANDLE_DESC
} trino_handle_type_t;

/* ========================================================================
 * Descriptor Area (IDA/ARP/IRD/ARD)
 * ======================================================================== */

#define MAX_DESCRIPTOR_RECORDS 32767
#define DEFAULT_DESCRIPTOR_RECORDS 256

/* Single descriptor record */
typedef struct {
    SQLSMALLINT sql_type;
    SQLSMALLINT c_type;
    SQLCHAR     column_name[TRINO_MAX_IDENTIFIER_LEN + 1];
    SQLCHAR     type_name[TRINO_MAX_IDENTIFIER_LEN + 1];
    SQLULEN     column_size;
    SQLPOINTER  data_ptr;
    SQLLEN     buffer_length;
    SQLLEN    *str_len_or_ind;
    SQLSMALLINT decimal_digits;
    SQLSMALLINT nullable;
    SQLLEN    row_offset;
} trino_desc_record_t;

/* Descriptor area */
typedef struct {
    trino_desc_record_t *records;
    SQLULEN              record_count;
    SQLULEN              alloc_count;
    SQLSMALLINT          bind_type;
    SQLULEN            *bind_offsets;
} trino_descriptor_t;

/* Create/destroy descriptor */
trino_descriptor_t *trino_desc_create(void);
void                trino_desc_destroy(trino_descriptor_t *desc);
SQLRETURN           trino_desc_set_field(trino_descriptor_t *desc, SQLUSMALLINT rec,
                                        SQLINTEGER field, SQLPOINTER value, SQLINTEGER str_len);
SQLRETURN           trino_desc_get_field(trino_descriptor_t *desc, SQLUSMALLINT rec,
                                        SQLINTEGER field, SQLPOINTER value, SQLINTEGER buffer_length,
                                        SQLINTEGER *str_len);

/* ========================================================================
 * Environment Handle
 * ======================================================================== */

typedef struct {
    trino_handle_type_t   type;
    SQLUINTEGER           odbc_version;
    SQLUINTEGER           connection_pooling;
    SQLUINTEGER           cp_match;
    SQLUINTEGER           access_mode;
    trino_diagnostics_t   diagnostics;
    pthread_mutex_t       mutex;
} trino_env_t;

/* ========================================================================
 * Connection Handle
 * ======================================================================== */

typedef struct {
    trino_handle_type_t   type;
    trino_env_t         *env;
    trino_diagnostics_t   diagnostics;
    pthread_mutex_t       mutex;

    /* Connection state */
    bool                  connected;
    bool                  autocommit;
    SQLUINTEGER           access_mode;
    SQLUINTEGER           login_timeout;
    SQLUINTEGER           query_timeout;
    SQLCHAR              *current_catalog;
    SQLCHAR              *current_schema;

    /* Server configuration */
    SQLCHAR              *server;
    SQLINTEGER            port;
    SQLCHAR              *user;
    SQLCHAR              *password;
    SQLCHAR              *catalog;
    SQLCHAR              *schema;
    SQLCHAR              *source;
    SQLCHAR              *client_tags_json;
    SQLCHAR              *session_properties_json;

    /* Authentication */
    SQLCHAR              *auth_type;    /* NONE, PASSWORD, CERTIFICATE, KERBEROS */
    bool                  ssl_enabled;
    SQLCHAR              *ssl_truststore;

    /* Transaction state */
    bool                  in_transaction;
    SQLUINTEGER           txn_isolation;   /* SQL_TXN_READ_COMMITTED etc. */
    SQLCHAR              *txn_savepoint;   /* current savepoint name */

    /* Cursor type */
    SQLUINTEGER           cursor_type;
    SQLUINTEGER           concurrency;

    /* Row limiting */
    SQLULEN               max_rows;

    /* Statement list */
    struct trino_stmt_s **statements;
    SQLULEN               stmt_count;
    SQLULEN               stmt_capacity;
} trino_conn_t;

/* ========================================================================
 * Statement Handle
 * ======================================================================== */

typedef struct trino_stmt_s {
    trino_handle_type_t    type;
    trino_conn_t         *conn;
    trino_diagnostics_t    diagnostics;
    pthread_mutex_t        mutex;

    /* SQL text */
    SQLCHAR              *sql_text;
    SQLINTEGER            sql_length;

    /* Statement state */
    bool                  prepared;
    bool                  executed;
    bool                  at_end;

    /* Parameters */
    SQLULEN               param_count;
    trino_descriptor_t   *ipd;       /* Internal Parameter Descriptor */

    /* Results */
    SQLULEN               column_count;
    trino_descriptor_t   *ird;       /* Internal Row Descriptor */
    trino_descriptor_t   *ard;       /* Application Row Descriptor */

    /* Result set */
    void                 *resultset;  /* opaque pointer to result set */
    SQLLEN                row_count;
    SQLULEN               current_row;

    /* Statement attributes */
    SQLUINTEGER           query_timeout;
    SQLUINTEGER           cursor_type;
    SQLUINTEGER           concurrency;
    SQLULEN               max_rows;
    SQLULEN               row_array_size;
    SQLUSMALLINT         *row_status;

    /* Trino query state */
    SQLCHAR              *query_id;
    SQLCHAR              *query_state;
    SQLULEN               rows_processed;
    SQLULEN               bytes_processed;
    SQLULEN               elapsed_time_ms;
} trino_stmt_t;

/* ========================================================================
 * Handle validation
 * ======================================================================== */

bool trino_env_valid(trino_env_t *env);
bool trino_conn_valid(trino_conn_t *conn);
bool trino_stmt_valid(trino_stmt_t *stmt);

#endif /* TRINO_ODBC_CORE_H */
