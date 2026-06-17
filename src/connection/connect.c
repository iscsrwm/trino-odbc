/* ODBC connection entry points: SQLConnect, SQLDriverConnect, SQLDisconnect.
 *
 * These are the functions a Driver Manager (unixODBC/iODBC) calls to establish
 * and tear down a connection. They translate the DM-facing arguments into a
 * trino_conn_config_t and delegate to the internal connection lifecycle in
 * connection.c.
 */
#include "trino_odbc/connection.h"
#include "trino_odbc/protocol.h"
#include "trino_odbc/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

/* Convert an ODBC wide-string argument (SQLWCHAR*, possibly SQL_NTS or
 * length-delimited in characters) into a freshly allocated UTF-8 C string.
 * Returns NULL for a NULL input. Caller frees. */
static char *odbc_wstrdup_utf8(const SQLWCHAR *wstr, SQLSMALLINT len)
{
    if (!wstr)
        return NULL;
    size_t wlen = (len == SQL_NTS) ? trino_wstrlen(wstr) : (len < 0 ? 0 : (size_t)len);
    if (wlen == 0) {
        char *empty = malloc(1);
        if (empty)
            empty[0] = '\0';
        return empty;
    }
    return trino_wchars_to_utf8(wstr, wlen);
}

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
    trino_log("SQLConnect: ENTRY handle=%p", (void *)connection_handle);

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
    /* trino_conn_connect sets a specific diagnostic on failure; keep it. */
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

    trino_log("SQLDriverConnect: ENTRY handle=%p", (void *)connection_handle);

    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn)) {
        trino_log("SQLDriverConnect: invalid handle (type mismatch)");
        return SQL_INVALID_HANDLE;
    }

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
    /* Redact the password before logging the connection string. */
    {
        char *redacted = strdup(conn_str);
        if (redacted) {
            char *p = redacted;
            while (*p) {
                if (strncasecmp(p, "Password=", 9) == 0) {
                    p += 9;
                    while (*p && *p != ';')
                        *p++ = '*';
                } else {
                    p++;
                }
            }
            trino_log("SQLDriverConnect: in_conn_str=%s", redacted);
            free(redacted);
        }
    }

    trino_conn_config_t config;
    SQLRETURN ret = trino_parse_conn_string((const SQLCHAR *)conn_str, &config);
    if (ret != SQL_SUCCESS) {
        free(conn_str);
        trino_diag_set_error(&conn->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "Invalid connection string");
        trino_log("SQLDriverConnect: parse failed");
        return SQL_ERROR;
    }

    ret = trino_conn_connect(conn, &config);
    if (ret != SQL_SUCCESS) {
        free(conn_str);
        /* trino_conn_connect already set a specific diagnostic (e.g. the curl
         * error or HTTP status); do not overwrite it. */
        trino_log("SQLDriverConnect: trino_conn_connect returned error");
        return SQL_ERROR;
    }
    trino_log("SQLDriverConnect: connected OK");

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

/* ========================================================================
 * Unicode (W) variants. The Windows ODBC Driver Manager calls the W-suffixed
 * entry points when the application uses the Unicode ODBC API (.NET's
 * OdbcConnection does). We convert the UTF-16 arguments to UTF-8 and delegate
 * to the ANSI implementations above, then convert any echoed-back string
 * (SQLDriverConnectW's out_conn_str) back to UTF-16.
 * ======================================================================== */

SQLRETURN SQLConnectW(SQLHDBC connection_handle, SQLWCHAR *server_name,
                      SQLSMALLINT name_length1, SQLWCHAR *user_name,
                      SQLSMALLINT name_length2, SQLWCHAR *authentication,
                      SQLSMALLINT name_length3)
{
    char *server = odbc_wstrdup_utf8(server_name, name_length1);
    char *user = odbc_wstrdup_utf8(user_name, name_length2);
    char *auth = odbc_wstrdup_utf8(authentication, name_length3);

    SQLRETURN ret = SQLConnect(
        connection_handle, (SQLCHAR *)server, server ? SQL_NTS : 0, (SQLCHAR *)user,
        user ? SQL_NTS : 0, (SQLCHAR *)auth, auth ? SQL_NTS : 0);

    free(server);
    free(user);
    free(auth);
    return ret;
}

SQLRETURN SQLDriverConnectW(SQLHDBC connection_handle, SQLHWND window_handle,
                            SQLWCHAR *in_conn_str, SQLSMALLINT in_conn_str_len,
                            SQLWCHAR *out_conn_str, SQLSMALLINT out_conn_str_max,
                            SQLSMALLINT *out_conn_str_len,
                            SQLUSMALLINT driver_completion)
{
    char *in_utf8 = odbc_wstrdup_utf8(in_conn_str, in_conn_str_len);
    trino_log("SQLDriverConnectW: entry");

    /* Collect the ANSI completed string into a local buffer, then widen it. */
    char ansi_out[2048];
    SQLSMALLINT ansi_out_len = 0;
    SQLRETURN ret = SQLDriverConnect(
        connection_handle, window_handle, (SQLCHAR *)in_utf8, in_utf8 ? SQL_NTS : 0,
        (SQLCHAR *)ansi_out, (SQLSMALLINT)sizeof(ansi_out), &ansi_out_len,
        driver_completion);

    free(in_utf8);

    trino_log("SQLDriverConnectW: SQLDriverConnect returned %d", (int)ret);
    if (ret == SQL_ERROR || ret == SQL_INVALID_HANDLE)
        return ret;

    /* Widen the echoed connection string back to UTF-16 for the caller. */
    if (ansi_out_len > 0) {
        size_t wlen = 0;
        SQLWCHAR *wout = trino_utf8_to_wchars(ansi_out, &wlen);
        if (out_conn_str && out_conn_str_max > 0) {
            size_t copy = wlen;
            if (copy > (size_t)(out_conn_str_max - 1)) {
                copy = (size_t)(out_conn_str_max - 1);
                ret = SQL_SUCCESS_WITH_INFO;
            }
            if (wout && copy > 0)
                memcpy(out_conn_str, wout, copy * sizeof(SQLWCHAR));
            if (out_conn_str)
                out_conn_str[copy] = 0;
        }
        if (out_conn_str_len)
            *out_conn_str_len = (SQLSMALLINT)wlen;
        free(wout);
    } else if (out_conn_str_len) {
        *out_conn_str_len = 0;
    }

    return ret;
}
