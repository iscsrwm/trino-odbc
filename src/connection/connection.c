#include "trino_odbc/connection.h"
#include "trino_odbc/compat.h"
#include "trino_odbc/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

/* ========================================================================
 * Connection string parser
 * ======================================================================== */

/* Parse a non-negative integer from a connection-string value. Returns the
 * parsed value, or `fallback` if the value is missing/empty/non-numeric or out
 * of the [0, INT_MAX] range. */
static long parse_uint_value(const char *value, long fallback)
{
    if (!value || !*value)
        return fallback;
    errno = 0;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == value || errno != 0 || v < 0 || v > INT_MAX)
        return fallback;
    while (*end && (*end == ' ' || *end == '\t'))
        end++;
    if (*end != '\0')
        return fallback;
    return v;
}

void trino_conn_config_defaults(trino_conn_config_t *config)
{
    memset(config, 0, sizeof(*config));
    strncpy((char *)config->server, "localhost", sizeof(config->server) - 1);
    config->port = 8080;
    strncpy((char *)config->auth_type, "NONE", sizeof(config->auth_type) - 1);
    config->ssl_enabled = false;
    config->ssl_verify = true; /* verify certificates by default */
    config->ssl_no_revoke = false;
    strncpy((char *)config->source, "trino-odbc", sizeof(config->source) - 1);
    config->query_timeout = 300;
    config->connect_timeout = 30;
}

/* Parse key=value;key=value connection string */
SQLRETURN trino_parse_conn_string(const SQLCHAR *conn_str, trino_conn_config_t *config)
{
    if (!conn_str || !config) {
        return SQL_ERROR;
    }

    trino_conn_config_defaults(config);

    char *str = strdup((const char *)conn_str);
    if (!str)
        return SQL_ERROR;

    char *saveptr = NULL;
    char *token = strtok_r(str, ";", &saveptr);

    while (token) {
        char *equals = strchr(token, '=');
        if (!equals) {
            token = strtok_r(NULL, ";", &saveptr);
            continue;
        }

        *equals = '\0';
        char *key = token;
        char *value = equals + 1;

        /* Trim whitespace */
        while (*key == ' ')
            key++;
        while (*value == ' ')
            value++;

        /* Case-insensitive key comparison */
        if (strcasecmp(key, "Server") == 0 || strcasecmp(key, "Host") == 0) {
            strncpy((char *)config->server, value, sizeof(config->server) - 1);
        } else if (strcasecmp(key, "Port") == 0) {
            config->port = (SQLINTEGER)parse_uint_value(value, config->port);
        } else if (strcasecmp(key, "User") == 0 || strcasecmp(key, "Username") == 0) {
            strncpy((char *)config->user, value, sizeof(config->user) - 1);
        } else if (strcasecmp(key, "Password") == 0) {
            strncpy((char *)config->password, value, sizeof(config->password) - 1);
        } else if (strcasecmp(key, "Catalog") == 0) {
            strncpy((char *)config->catalog, value, sizeof(config->catalog) - 1);
        } else if (strcasecmp(key, "Schema") == 0) {
            strncpy((char *)config->schema, value, sizeof(config->schema) - 1);
        } else if (strcasecmp(key, "Authentication") == 0 ||
                   strcasecmp(key, "AuthType") == 0) {
            strncpy((char *)config->auth_type, value, sizeof(config->auth_type) - 1);
        } else if (strcasecmp(key, "SSL") == 0) {
            config->ssl_enabled =
                (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0 ||
                 strcmp(value, "1") == 0);
        } else if (strcasecmp(key, "SSLVerify") == 0) {
            /* SSLVerify=false disables peer/host certificate verification. */
            config->ssl_verify =
                !(strcasecmp(value, "false") == 0 || strcasecmp(value, "no") == 0 ||
                  strcmp(value, "0") == 0);
        } else if (strcasecmp(key, "SSLNoRevoke") == 0) {
            /* SSLNoRevoke=true skips the certificate revocation check (fixes
             * CRYPT_E_REVOCATION_OFFLINE on networks where the CRL/OCSP server
             * is unreachable). */
            config->ssl_no_revoke =
                (strcasecmp(value, "true") == 0 || strcasecmp(value, "yes") == 0 ||
                 strcmp(value, "1") == 0);
        } else if (strcasecmp(key, "SSLTrustStoreCertificate") == 0) {
            strncpy((char *)config->ssl_truststore, value,
                    sizeof(config->ssl_truststore) - 1);
        } else if (strcasecmp(key, "Source") == 0) {
            strncpy((char *)config->source, value, sizeof(config->source) - 1);
        } else if (strcasecmp(key, "ClientTags") == 0) {
            strncpy((char *)config->client_tags, value, sizeof(config->client_tags) - 1);
        } else if (strcasecmp(key, "SessionProperties") == 0) {
            strncpy((char *)config->session_properties, value,
                    sizeof(config->session_properties) - 1);
        } else if (strcasecmp(key, "QueryTimeout") == 0) {
            config->query_timeout =
                (SQLUINTEGER)parse_uint_value(value, config->query_timeout);
        } else if (strcasecmp(key, "ConnectTimeout") == 0) {
            config->connect_timeout =
                (SQLUINTEGER)parse_uint_value(value, config->connect_timeout);
        }

        token = strtok_r(NULL, ";", &saveptr);
    }

    free(str);
    return SQL_SUCCESS;
}

/* ========================================================================
 * Connection lifecycle
 * ======================================================================== */

trino_conn_t *trino_conn_create(trino_env_t *env)
{
    trino_conn_t *conn = calloc(1, sizeof(*conn));
    if (!conn)
        return NULL;

    conn->type = TRINO_HANDLE_DBC;
    conn->env = env;
    conn->autocommit = true;
    conn->access_mode = SQL_MODE_READ_WRITE;
    conn->login_timeout = 30;
    conn->query_timeout = 300;
    conn->cursor_type = SQL_CURSOR_FORWARD_ONLY;
    conn->concurrency = SQL_CONCUR_READ_ONLY;
    conn->max_rows = 0; /* unlimited */
    conn->connected = false;

    /* Transaction defaults */
    conn->in_transaction = false;
    conn->txn_isolation = SQL_TXN_READ_COMMITTED;
    conn->txn_savepoint = NULL;

    trino_diag_init(&conn->diagnostics);
    pthread_mutex_init(&conn->mutex, NULL);

    /* Set defaults */
    conn->server = strdup("localhost");
    conn->port = 8080;
    conn->auth_type = strdup("NONE");
    conn->source = strdup("trino-odbc");

    return conn;
}

void trino_conn_destroy(trino_conn_t *conn)
{
    if (!conn)
        return;

    if (conn->connected) {
        trino_conn_disconnect(conn);
    }

    if (conn->http_client) {
        trino_http_client_destroy(conn->http_client);
        conn->http_client = NULL;
    }

    free(conn->server);
    free(conn->user);
    free(conn->password);
    free(conn->catalog);
    free(conn->schema);
    free(conn->source);
    free(conn->current_catalog);
    free(conn->current_schema);
    free(conn->auth_type);
    free(conn->ssl_truststore);
    free(conn->client_tags_json);
    free(conn->session_properties_json);
    free(conn->txn_savepoint);

    /* Free statement list */
    if (conn->statements) {
        free(conn->statements);
    }

    pthread_mutex_destroy(&conn->mutex);
    free(conn);
}

/* ========================================================================
 * Connect to Trino server
 * ======================================================================== */

SQLRETURN trino_conn_connect(trino_conn_t *conn, const trino_conn_config_t *config)
{
    if (!conn || !config)
        return SQL_ERROR;

    pthread_mutex_lock(&conn->mutex);

    /* Free old values if reconnecting */
    free(conn->server);
    free(conn->user);
    free(conn->password);
    free(conn->catalog);
    free(conn->schema);
    free(conn->source);
    free(conn->auth_type);
    free(conn->ssl_truststore);

    /* Copy config */
    conn->server = strdup((char *)config->server);
    conn->port = config->port;
    conn->user = strlen((char *)config->user) > 0 ? strdup((char *)config->user) : NULL;
    conn->password =
        strlen((char *)config->password) > 0 ? strdup((char *)config->password) : NULL;
    conn->catalog =
        strlen((char *)config->catalog) > 0 ? strdup((char *)config->catalog) : NULL;
    conn->schema =
        strlen((char *)config->schema) > 0 ? strdup((char *)config->schema) : NULL;
    conn->source = strdup((char *)config->source);
    conn->auth_type = strdup((char *)config->auth_type);
    conn->ssl_enabled = config->ssl_enabled;
    conn->ssl_verify = config->ssl_verify;
    conn->ssl_no_revoke = config->ssl_no_revoke;
    conn->ssl_truststore = strlen((char *)config->ssl_truststore) > 0
                               ? strdup((char *)config->ssl_truststore)
                               : NULL;

    conn->connected = true;

    pthread_mutex_unlock(&conn->mutex);

    /* Validate connectivity against the server so the caller gets a real error
     * instead of a connection that only fails later. get_http_client locks the
     * connection mutex, so this must run after the unlock above. */
    trino_http_client_t *client = trino_conn_get_http_client(conn);
    if (!client) {
        pthread_mutex_lock(&conn->mutex);
        conn->connected = false;
        pthread_mutex_unlock(&conn->mutex);
        trino_diag_set_error(&conn->diagnostics, TRINO_SQLSTATE_LOGIN_FAILED, 0,
                             "Failed to initialize HTTP client");
        return SQL_ERROR;
    }

    trino_log("trino_conn_connect: validating server=%s port=%d ssl=%d auth=%s",
              conn->server ? conn->server : "(null)", (int)conn->port,
              (int)conn->ssl_enabled, conn->auth_type ? conn->auth_type : "(null)");

    char err[512] = {0};
    if (trino_http_client_validate(client, err, sizeof(err)) != SQL_SUCCESS) {
        pthread_mutex_lock(&conn->mutex);
        conn->connected = false;
        if (conn->http_client) {
            trino_http_client_destroy(conn->http_client);
            conn->http_client = NULL;
        }
        pthread_mutex_unlock(&conn->mutex);
        trino_diag_set_error(&conn->diagnostics, TRINO_SQLSTATE_LOGIN_FAILED, 0,
                             err[0] ? err : "Failed to connect to Trino server");
        trino_log("trino_conn_connect: validate FAILED: %s",
                  err[0] ? err : "(no detail)");
        return SQL_ERROR;
    }

    trino_log("trino_conn_connect: validate OK");
    return SQL_SUCCESS;
}

SQLRETURN trino_conn_disconnect(trino_conn_t *conn)
{
    if (!conn)
        return SQL_ERROR;

    pthread_mutex_lock(&conn->mutex);
    conn->connected = false;
    /* Drop the cached HTTP client so a subsequent reconnect rebuilds it with
     * the new configuration. */
    if (conn->http_client) {
        trino_http_client_destroy(conn->http_client);
        conn->http_client = NULL;
    }
    pthread_mutex_unlock(&conn->mutex);

    return SQL_SUCCESS;
}

/* ========================================================================
 * Connection attributes
 * ======================================================================== */

SQLRETURN trino_conn_set_attr(trino_conn_t *conn, SQLINTEGER attr, SQLPOINTER value,
                              SQLINTEGER str_len)
{
    if (!conn)
        return SQL_ERROR;

    pthread_mutex_lock(&conn->mutex);

    switch (attr) {
        case SQL_ATTR_AUTOCOMMIT: conn->autocommit = (*(SQLUINTEGER *)value != 0); break;

        case SQL_ATTR_ACCESS_MODE: conn->access_mode = *(SQLUINTEGER *)value; break;

        case SQL_ATTR_CURRENT_CATALOG: {
            free(conn->current_catalog);
            if (value) {
                conn->current_catalog = strndup((char *)value, (size_t)str_len);
            } else {
                conn->current_catalog = NULL;
            }
            break;
        }

        case SQL_ATTR_LOGIN_TIMEOUT: conn->login_timeout = *(SQLUINTEGER *)value; break;

        case SQL_ATTR_QUERY_TIMEOUT: conn->query_timeout = *(SQLUINTEGER *)value; break;

        default: break;
    }

    pthread_mutex_unlock(&conn->mutex);
    return SQL_SUCCESS;
}

SQLRETURN trino_conn_get_attr(trino_conn_t *conn, SQLINTEGER attr, SQLPOINTER value,
                              SQLINTEGER buffer_length, SQLINTEGER *str_len)
{
    if (!conn || !value)
        return SQL_ERROR;

    pthread_mutex_lock(&conn->mutex);

    switch (attr) {
        case SQL_ATTR_AUTOCOMMIT:
            *(SQLUINTEGER *)value =
                conn->autocommit ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_ACCESS_MODE:
            *(SQLUINTEGER *)value = conn->access_mode;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CURRENT_CATALOG:
            if (conn->current_catalog) {
                strncpy((char *)value, conn->current_catalog, (size_t)buffer_length - 1);
                ((char *)value)[buffer_length - 1] = '\0';
                if (str_len)
                    *str_len = (SQLINTEGER)strlen((char *)value);
            } else {
                if (str_len)
                    *str_len = 0;
            }
            break;

        case SQL_ATTR_LOGIN_TIMEOUT:
            *(SQLUINTEGER *)value = conn->login_timeout;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_QUERY_TIMEOUT:
            *(SQLUINTEGER *)value = conn->query_timeout;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        default: pthread_mutex_unlock(&conn->mutex); return SQL_SUCCESS;
    }

    pthread_mutex_unlock(&conn->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * HTTP client accessor
 * ======================================================================== */

trino_http_client_t *trino_conn_get_http_client(trino_conn_t *conn)
{
    if (!conn)
        return NULL;

    /* Return the cached client if it has already been created. The client is
     * owned by the connection and reused across statements/pages so that the
     * underlying TCP/TLS connection (and curl handle) can be reused. */
    if (conn->http_client) {
        return conn->http_client;
    }

    trino_http_client_t *client = trino_http_client_create();
    if (!client)
        return NULL;

    /* TLS verification options (read by configure when ssl is enabled). */
    client->ssl_verify = conn->ssl_verify;
    client->ssl_no_revoke = conn->ssl_no_revoke;

    SQLRETURN ret = trino_http_client_configure(
        client, conn->server, conn->port, conn->user, conn->password, conn->auth_type,
        conn->ssl_enabled, conn->ssl_truststore, conn->client_tags_json,
        conn->session_properties_json, conn->source);

    if (ret != SQL_SUCCESS) {
        trino_http_client_destroy(client);
        return NULL;
    }

    conn->http_client = client;
    return client;
}

/* ========================================================================
 * Statement tracking
 * ======================================================================== */

void trino_conn_register_stmt(trino_conn_t *conn, trino_stmt_t *stmt)
{
    if (!conn || !stmt)
        return;

    pthread_mutex_lock(&conn->mutex);

    if (conn->stmt_count >= conn->stmt_capacity) {
        SQLULEN new_cap = conn->stmt_capacity == 0 ? 8 : conn->stmt_capacity * 2;
        trino_stmt_t **new_arr =
            realloc(conn->statements, new_cap * sizeof(trino_stmt_t *));
        if (new_arr) {
            conn->statements = new_arr;
            conn->stmt_capacity = new_cap;
        }
    }

    if (conn->stmt_count < conn->stmt_capacity) {
        conn->statements[conn->stmt_count++] = stmt;
    }

    pthread_mutex_unlock(&conn->mutex);
}

void trino_conn_unregister_stmt(trino_conn_t *conn, trino_stmt_t *stmt)
{
    if (!conn || !stmt)
        return;

    pthread_mutex_lock(&conn->mutex);

    for (SQLULEN i = 0; i < conn->stmt_count; i++) {
        if (conn->statements[i] == stmt) {
            /* Shift remaining statements down */
            memmove(&conn->statements[i], &conn->statements[i + 1],
                    (conn->stmt_count - i - 1) * sizeof(trino_stmt_t *));
            conn->stmt_count--;
            break;
        }
    }

    pthread_mutex_unlock(&conn->mutex);
}
