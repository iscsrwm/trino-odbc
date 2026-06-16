/* ODBC connection entry points: SQLConnect, SQLDriverConnect, SQLDisconnect.
 *
 * These are the functions a Driver Manager (unixODBC/iODBC) calls to establish
 * and tear down a connection. They translate the DM-facing arguments into a
 * trino_conn_config_t and delegate to the internal connection lifecycle in
 * connection.c.
 */
#include "trino_odbc/connection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

/* Resolve an ODBC string argument that may be NUL-terminated (SQL_NTS) or
 * length-delimited into a freshly allocated NUL-terminated C string. Returns
 * NULL for a NULL input (caller treats as absent). */
static char *odbc_strdup(const SQLCHAR *str, SQLSMALLINT len)
{
    if (!str)
        return NULL;
    if (len == SQL_NTS) {
        return strdup((const char *)str);
    }
    if (len < 0)
        return NULL;
    char *out = malloc((size_t)len + 1);
    if (!out)
        return NULL;
    memcpy(out, str, (size_t)len);
    out[len] = '\0';
    return out;
}

/* ========================================================================
 * SQLConnect — connect using a DSN (or server name) + user + auth
 * ======================================================================== */

SQLRETURN SQLConnect(SQLHDBC connection_handle, SQLCHAR *server_name,
                     SQLSMALLINT name_length1, SQLCHAR *user_name,
                     SQLSMALLINT name_length2, SQLCHAR *authentication,
                     SQLSMALLINT name_length3)
{
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    if (conn->connected) {
        trino_diag_set_error(&conn->diagnostics, "08002", 0, "Connection name in use");
        return SQL_ERROR;
    }

    char *server = odbc_strdup(server_name, name_length1);
    char *user = odbc_strdup(user_name, name_length2);
    char *password = odbc_strdup(authentication, name_length3);

    trino_conn_config_t config;
    trino_conn_config_defaults(&config);

    /* The "server name" is treated as a host (or host:port). A full DSN lookup
     * against odbc.ini would be performed by the driver manager; if a bare DSN
     * name is passed we fall back to using it as the host. */
    if (server && *server) {
        char *colon = strrchr(server, ':');
        if (colon) {
            *colon = '\0';
            errno = 0;
            char *end = NULL;
            long p = strtol(colon + 1, &end, 10);
            if (end != colon + 1 && errno == 0 && p > 0 && p <= 65535) {
                config.port = (SQLINTEGER)p;
            }
        }
        strncpy((char *)config.server, server, sizeof(config.server) - 1);
        config.server[sizeof(config.server) - 1] = '\0';
    }
    if (user && *user) {
        strncpy((char *)config.user, user, sizeof(config.user) - 1);
        config.user[sizeof(config.user) - 1] = '\0';
    }
    if (password && *password) {
        strncpy((char *)config.password, password, sizeof(config.password) - 1);
        config.password[sizeof(config.password) - 1] = '\0';
        strncpy((char *)config.auth_type, "PASSWORD", sizeof(config.auth_type) - 1);
    }

    free(server);
    free(user);
    free(password);

    SQLRETURN ret = trino_conn_connect(conn, &config);
    if (ret != SQL_SUCCESS) {
        trino_diag_set_error(&conn->diagnostics, TRINO_SQLSTATE_LOGIN_FAILED, 0,
                             "Failed to connect to Trino server");
    }
    return ret;
}

/* ========================================================================
 * SQLDriverConnect — connect using a full connection string
 * ======================================================================== */

SQLRETURN SQLDriverConnect(SQLHDBC connection_handle, SQLHWND window_handle,
                           SQLCHAR *in_conn_str, SQLSMALLINT in_conn_str_len,
                           SQLCHAR *out_conn_str, SQLSMALLINT out_conn_str_max,
                           SQLSMALLINT *out_conn_str_len, SQLUSMALLINT driver_completion)
{
    (void)window_handle;
    (void)driver_completion; /* No interactive prompting (no GUI). */

    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    if (conn->connected) {
        trino_diag_set_error(&conn->diagnostics, "08002", 0, "Connection name in use");
        return SQL_ERROR;
    }

    char *conn_str = odbc_strdup(in_conn_str, in_conn_str_len);
    if (!conn_str) {
        trino_diag_set_error(&conn->diagnostics, TRINO_SQLSTATE_INVALID_STR_LEN, 0,
                             "Missing connection string");
        return SQL_ERROR;
    }

    trino_conn_config_t config;
    SQLRETURN ret = trino_parse_conn_string((const SQLCHAR *)conn_str, &config);
    if (ret != SQL_SUCCESS) {
        free(conn_str);
        trino_diag_set_error(&conn->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "Invalid connection string");
        return SQL_ERROR;
    }

    ret = trino_conn_connect(conn, &config);
    if (ret != SQL_SUCCESS) {
        free(conn_str);
        trino_diag_set_error(&conn->diagnostics, TRINO_SQLSTATE_LOGIN_FAILED, 0,
                             "Failed to connect to Trino server");
        return SQL_ERROR;
    }

    /* Echo the (completed) connection string back to the caller. We return the
     * input string as-is, which satisfies applications that store it for
     * later reconnection. */
    if (out_conn_str && out_conn_str_max > 0) {
        size_t copy_len = strlen(conn_str);
        if (copy_len > (size_t)(out_conn_str_max - 1)) {
            copy_len = (size_t)(out_conn_str_max - 1);
            ret = SQL_SUCCESS_WITH_INFO;
        }
        memcpy(out_conn_str, conn_str, copy_len);
        out_conn_str[copy_len] = '\0';
        if (out_conn_str_len)
            *out_conn_str_len = (SQLSMALLINT)copy_len;
    } else if (out_conn_str_len) {
        *out_conn_str_len = (SQLSMALLINT)strlen(conn_str);
    }

    free(conn_str);
    return ret;
}

/* ========================================================================
 * SQLDisconnect
 * ======================================================================== */

SQLRETURN SQLDisconnect(SQLHDBC connection_handle)
{
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    if (!conn->connected) {
        trino_diag_set_error(&conn->diagnostics, "08003", 0, "Connection not open");
        return SQL_ERROR;
    }

    return trino_conn_disconnect(conn);
}
