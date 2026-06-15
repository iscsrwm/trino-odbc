#ifndef TRINO_ODBC_PROTOCOL_H
#define TRINO_ODBC_PROTOCOL_H

#include "trino_odbc.h"
#include <curl/curl.h>

/* ========================================================================
 * Trino Query States
 * ======================================================================== */

typedef enum {
    TRINO_QUERY_STATE_QUEUE,
    TRINO_QUERY_STATE_PLANNING,
    TRINO_QUERY_STATE_STARTING,
    TRINO_QUERY_STATE_RUNNING,
    TRINO_QUERY_STATE_FINISHED,
    TRINO_QUERY_STATE_FAILED,
    TRINO_QUERY_STATE_CANCELLED
} trino_query_state_t;

/* ========================================================================
 * Column metadata from Trino
 * ======================================================================== */

#define TRINO_MAX_TYPE_NAME 128

typedef struct {
    SQLCHAR   name[SQL_MAX_IDENTIFIER_LEN + 1];
    SQLCHAR   type[TRINO_MAX_TYPE_NAME];
    SQLSMALLINT odbc_type;
    SQLULEN   column_size;
    SQLSMALLINT decimal_digits;
    SQLSMALLINT nullable;
} trino_column_meta_t;

/* ========================================================================
 * Query results from Trino
 * ======================================================================== */

#define TRINO_MAX_ROWS_PER_BATCH 10000

typedef struct {
    /* Query identification */
    SQLCHAR   query_id[64];
    SQLCHAR  *next_uri;          /* NULL when done */
    trino_query_state_t state;

    /* Column metadata */
    trino_column_meta_t *columns;
    SQLULEN              column_count;

    /* Data rows — each cell is a null-terminated string */
    SQLCHAR         ***rows;     /* rows[row][col] */
    SQLULEN           row_count;
    SQLULEN           row_capacity;

    /* Error info (if query failed) */
    bool              has_error;
    SQLCHAR          *error_name;
    SQLCHAR          *error_message;
    SQLCHAR          *error_type;
    SQLCHAR          *error_uri;
    SQLULEN           error_code;

    /* Query statistics */
    SQLULEN           rows_processed;
    SQLULEN           bytes_processed;
    SQLDOUBLE         elapsed_time;
    SQLCHAR          *stats_uri;

    /* Memory info */
    SQLCHAR          *task_info_uri;
} trino_query_results_t;

/* ========================================================================
 * HTTP Client
 * ======================================================================== */

typedef struct {
    CURL          *easy_handle;
    CURLSH       *share_handle;   /* shared handle for connection reuse */

    /* Connection config */
    SQLCHAR      *server_url;     /* e.g., "http://localhost:8080" */
    SQLCHAR      *user;
    SQLCHAR      *password;
    SQLCHAR      *auth_type;
    bool          ssl_enabled;
    SQLCHAR      *ssl_truststore;
    SQLCHAR      *client_tags_json;
    SQLCHAR      *session_properties_json;
    SQLCHAR      *source;

    /* Timeouts */
    SQLUINTEGER   connect_timeout;
    SQLUINTEGER   request_timeout;

    /* Callback for cancellation check */
    int          (*cancel_check)(void *);
    void        *cancel_check_ctx;
} trino_http_client_t;

/* Initialize / destroy HTTP client */
trino_http_client_t *trino_http_client_create(void);
void                 trino_http_client_destroy(trino_http_client_t *client);

/* Configure client from connection */
SQLRETURN trino_http_client_configure(trino_http_client_t *client,
                                     const char *server, SQLINTEGER port,
                                     const char *user, const char *password,
                                     const char *auth_type, bool ssl,
                                     const char *ssl_truststore,
                                     const char *client_tags_json,
                                     const char *session_properties_json,
                                     const char *source);

/* Execute a SQL query — returns QueryResults (caller must free) */
trino_query_results_t *trino_http_client_query(trino_http_client_t *client,
                                               const SQLCHAR *sql,
                                               SQLRETURN *retcode);

/* Fetch next batch of results */
SQLRETURN trino_http_client_fetch_next(trino_http_client_t *client,
                                       trino_query_results_t *results);

/* Kill a running query */
SQLRETURN trino_http_client_kill_query(trino_http_client_t *client,
                                       const SQLCHAR *query_id);

/* Free query results */
void trino_query_results_free(trino_query_results_t *results);

/* Map Trino type string to ODBC SQL type */
SQLSMALLINT trino_type_to_odbc_type(const char *trino_type);

/* Get human-readable name for Trino type */
const char *trino_type_name(SQLSMALLINT odbc_type);

/* Parse column metadata from JSON columns array string */
trino_column_meta_t *trino_parse_columns(const char *json, SQLULEN *column_count);

#endif /* TRINO_ODBC_PROTOCOL_H */
