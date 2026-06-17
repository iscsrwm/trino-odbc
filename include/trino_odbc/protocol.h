#ifndef TRINO_ODBC_PROTOCOL_H
#define TRINO_ODBC_PROTOCOL_H

#include "trino_odbc.h"
#include <curl/curl.h>
#include <json-c/json.h>

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
    SQLCHAR name[TRINO_MAX_IDENTIFIER_LEN + 1];
    SQLCHAR type[TRINO_MAX_TYPE_NAME];
    SQLSMALLINT odbc_type;
    SQLULEN column_size;
    SQLSMALLINT decimal_digits;
    SQLSMALLINT nullable;
} trino_column_meta_t;

/* ========================================================================
 * Query results from Trino
 * ======================================================================== */

#define TRINO_MAX_ROWS_PER_BATCH 10000

typedef struct {
    /* Query identification (C strings). */
    char query_id[64];
    char *next_uri; /* NULL when done */
    trino_query_state_t state;

    /* Column metadata */
    trino_column_meta_t *columns;
    SQLULEN column_count;

    /* Data rows — each cell is a NUL-terminated C string (NULL == SQL NULL). */
    char ***rows; /* rows[row][col] */
    SQLULEN row_count;
    SQLULEN row_capacity;

    /* Error info (if query failed) */
    bool has_error;
    char *error_name;
    char *error_message;
    char *error_type;
    char *error_uri;
    SQLULEN error_code;

    /* Query statistics */
    SQLULEN rows_processed;
    SQLULEN bytes_processed;
    SQLDOUBLE elapsed_time;
    char *stats_uri;

    /* Memory info */
    char *task_info_uri;
} trino_query_results_t;

/* ========================================================================
 * HTTP Client
 * ======================================================================== */

typedef struct trino_http_client_s {
    CURL *easy_handle;
    CURLSH *share_handle; /* shared handle for connection reuse */

    /* Connection config (heap-allocated C strings). */
    char *server_url; /* e.g., "http://localhost:8080" */
    char *user;
    char *password;
    char *auth_type;
    bool ssl_enabled;
    bool ssl_verify;    /* verify peer/host cert (default true) */
    bool ssl_no_revoke; /* skip cert revocation check (default false) */
    char *ssl_truststore;
    char *client_tags_json;
    char *session_properties_json;
    char *source;

    /* Timeouts */
    SQLUINTEGER connect_timeout;
    SQLUINTEGER request_timeout;

    /* Callback for cancellation check */
    int (*cancel_check)(void *);
    void *cancel_check_ctx;
} trino_http_client_t;

/* Initialize / destroy HTTP client */
trino_http_client_t *trino_http_client_create(void);
void trino_http_client_destroy(trino_http_client_t *client);

/* ------------------------------------------------------------------------
 * Connection pool
 *
 * A process-wide curl share handle pools the underlying TCP/TLS connections,
 * DNS cache, and TLS sessions across all driver connections, so repeated
 * statements/queries reuse sockets instead of reconnecting. Reference-counted:
 * acquired when an HTTP client is created and released when it is destroyed.
 * Thread-safe.
 * ------------------------------------------------------------------------ */

/* Acquire the shared curl handle (initializing the pool on first use).
 * Returns NULL if the pool could not be created. */
CURLSH *trino_http_pool_acquire(void);

/* Release a previously acquired reference; tears down the pool at zero. */
void trino_http_pool_release(void);

/* ------------------------------------------------------------------------
 * Test transport hook
 *
 * When set to a non-NULL function, the client routes requests through this
 * hook instead of performing real HTTP via libcurl. This exists solely to
 * enable end-to-end tests of the execute/fetch/getdata path without a live
 * Trino server. In production the hook is NULL and libcurl is used.
 *
 * The hook receives the HTTP method ("POST"/"GET"), the URL, and the request
 * body (may be NULL), and must return a heap-allocated, NUL-terminated
 * response body that the caller will free(), or NULL to simulate a transport
 * failure. `user_ctx` is the value passed to trino_http_set_test_transport.
 * ------------------------------------------------------------------------ */
typedef char *(*trino_http_transport_fn)(const char *method, const char *url,
                                         const char *body, void *user_ctx);

void trino_http_set_test_transport(trino_http_transport_fn fn, void *user_ctx);

/* Sanitize a user-controlled HTTP header value (strip CR/LF, truncate to
 * out_size-1). Exposed for testing; used internally when building requests. */
void trino_http_sanitize_header_value(const char *value, char *out, size_t out_size);

/* Configure client from connection */
SQLRETURN trino_http_client_configure(trino_http_client_t *client, const char *server,
                                      SQLINTEGER port, const char *user,
                                      const char *password, const char *auth_type,
                                      bool ssl, const char *ssl_truststore,
                                      const char *client_tags_json,
                                      const char *session_properties_json,
                                      const char *source);

/* Execute a SQL query — returns QueryResults (caller must free) */
trino_query_results_t *trino_http_client_query(trino_http_client_t *client,
                                               const SQLCHAR *sql, SQLRETURN *retcode);

/* Fetch next batch of results */
SQLRETURN trino_http_client_fetch_next(trino_http_client_t *client,
                                       trino_query_results_t *results);

/* Kill a running query */
SQLRETURN trino_http_client_kill_query(trino_http_client_t *client, const char *query_id);

/* Free query results */
void trino_query_results_free(trino_query_results_t *results);

/* Map Trino type string to ODBC SQL type */
SQLSMALLINT trino_type_to_odbc_type(const char *trino_type);

/* Get human-readable name for Trino type */
const char *trino_type_name(SQLSMALLINT odbc_type);

/* Parse column metadata from QueryResults JSON using json-c */
trino_column_meta_t *trino_parse_columns(const char *json, SQLULEN *column_count);

/* Parse column metadata from JSON columns array using json-c */
trino_column_meta_t *trino_parse_columns_jsonc(json_object *columns_array,
                                               SQLULEN *column_count);

/* Parse a full Trino QueryResults JSON document into a results structure.
 *
 * Populates query id, nextUri, state, error info, stats, columns (only if not
 * already set on a prior page), and appends any data rows. Safe to call
 * repeatedly for successive pages: rows accumulate and metadata is updated.
 *
 * Returns SQL_SUCCESS on a well-formed (non-error) response, SQL_ERROR if the
 * response reports a Trino error (error fields are populated), or SQL_ERROR on
 * a malformed document. `json_text` must be NUL-terminated. */
SQLRETURN trino_parse_query_response(const char *json_text,
                                     trino_query_results_t *results);

/* Free only the row data of a results structure (used between pages). */
void trino_query_results_free_rows(trino_query_results_t *results);

#endif /* TRINO_ODBC_PROTOCOL_H */

/* Wide character string helpers (from wchar.c) */
char *trino_wchars_to_utf8(const SQLWCHAR *wstr, size_t wlen);
SQLWCHAR *trino_utf8_to_wchars(const char *str, size_t *out_wlen);
size_t trino_wstrlen(const SQLWCHAR *wstr);
