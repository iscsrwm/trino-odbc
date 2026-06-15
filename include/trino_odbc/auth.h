#ifndef TRINO_ODBC_AUTH_H
#define TRINO_ODBC_AUTH_H

#include "trino_odbc.h"
#include <curl/curl.h>

/* ========================================================================
 * Authentication methods
 * ======================================================================== */

typedef enum {
    TRINO_AUTH_NONE,
    TRINO_AUTH_PASSWORD,
    TRINO_AUTH_CERTIFICATE,
    TRINO_AUTH_KERBEROS
} trino_auth_method_t;

/* Parse auth type string */
trino_auth_method_t trino_auth_parse(const char *auth_type);

/* Apply authentication to a curl easy handle */
SQLRETURN trino_auth_apply(CURL *handle, trino_auth_method_t method,
                           const char *user, const char *password,
                           const char *ssl_truststore);

#endif /* TRINO_ODBC_AUTH_H */
