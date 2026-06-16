#ifndef TRINO_ODBC_CONNECTION_H
#define TRINO_ODBC_CONNECTION_H

#include "trino_odbc/core.h"
#include "trino_odbc/protocol.h"

/* ========================================================================
 * Connection string parser
 * ======================================================================== */

typedef struct {
    SQLCHAR server[256];
    SQLINTEGER port;
    SQLCHAR user[256];
    SQLCHAR password[512];
    SQLCHAR catalog[256];
    SQLCHAR schema[256];
    SQLCHAR auth_type[32];
    bool ssl_enabled;
    SQLCHAR ssl_truststore[1024];
    SQLCHAR source[128];
    SQLCHAR client_tags[1024];
    SQLCHAR session_properties[1024];
    SQLUINTEGER query_timeout;
    SQLUINTEGER connect_timeout;
} trino_conn_config_t;

/* Parse a connection string (key=value;key=value format) */
SQLRETURN trino_parse_conn_string(const SQLCHAR *conn_str, trino_conn_config_t *config);

/* Initialize default config */
void trino_conn_config_defaults(trino_conn_config_t *config);

/* ========================================================================
 * Connection lifecycle
 * ======================================================================== */

trino_conn_t *trino_conn_create(trino_env_t *env);
void trino_conn_destroy(trino_conn_t *conn);

/* Connect to Trino server */
SQLRETURN trino_conn_connect(trino_conn_t *conn, const trino_conn_config_t *config);

/* Disconnect */
SQLRETURN trino_conn_disconnect(trino_conn_t *conn);

/* Set/get connection attributes */
SQLRETURN trino_conn_set_attr(trino_conn_t *conn, SQLINTEGER attr, SQLPOINTER value,
                              SQLINTEGER str_len);
SQLRETURN trino_conn_get_attr(trino_conn_t *conn, SQLINTEGER attr, SQLPOINTER value,
                              SQLINTEGER buffer_length, SQLINTEGER *str_len);

/* Get HTTP client for this connection */
trino_http_client_t *trino_conn_get_http_client(trino_conn_t *conn);

/* Register/unregister statement */
void trino_conn_register_stmt(trino_conn_t *conn, trino_stmt_t *stmt);
void trino_conn_unregister_stmt(trino_conn_t *conn, trino_stmt_t *stmt);

#endif /* TRINO_ODBC_CONNECTION_H */
