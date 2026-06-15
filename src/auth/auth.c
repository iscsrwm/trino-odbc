#include "trino_odbc/auth.h"
#include <string.h>
#include <strings.h>

trino_auth_method_t trino_auth_parse(const char *auth_type)
{
    if (!auth_type) return TRINO_AUTH_NONE;

    if (strcasecmp(auth_type, "NONE") == 0) return TRINO_AUTH_NONE;
    if (strcasecmp(auth_type, "PASSWORD") == 0 ||
        strcasecmp(auth_type, "BASIC") == 0 ||
        strcasecmp(auth_type, "LDAP") == 0) return TRINO_AUTH_PASSWORD;
    if (strcasecmp(auth_type, "CERTIFICATE") == 0 ||
        strcasecmp(auth_type, "CLIENT-CERT") == 0) return TRINO_AUTH_CERTIFICATE;
    if (strcasecmp(auth_type, "KERBEROS") == 0 ||
        strcasecmp(auth_type, "SPNEGO") == 0) return TRINO_AUTH_KERBEROS;

    return TRINO_AUTH_NONE;
}

SQLRETURN trino_auth_apply(CURL *handle, trino_auth_method_t method,
                           const char *user, const char *password,
                           const char *ssl_truststore)
{
    if (!handle) return SQL_ERROR;

    switch (method) {
        case TRINO_AUTH_NONE:
            /* No authentication needed */
            curl_easy_setopt(handle, CURLOPT_USERPWD, NULL);
            break;

        case TRINO_AUTH_PASSWORD:
            if (user && strlen(user) > 0) {
                /* Build user:password string */
                static char userpass[1024];
                snprintf(userpass, sizeof(userpass), "%s:%s",
                         user, password ? password : "");
                curl_easy_setopt(handle, CURLOPT_USERPWD, userpass);
                curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
            }
            break;

        case TRINO_AUTH_CERTIFICATE:
            /* Certificate auth is handled at the SSL layer */
            if (ssl_truststore) {
                curl_easy_setopt(handle, CURLOPT_CAINFO, ssl_truststore);
            }
            /* Client cert would be set via CURLOPT_SSLCERT/CURLOPT_SSLKEY */
            break;

        case TRINO_AUTH_KERBEROS:
#ifdef CURLAUTH_SPNEGO
            curl_easy_setopt(handle, CURLOPT_HTTPAUTH, CURLAUTH_SPNEGO);
#else
            /* SPNEGO not available in this curl build */
            (void)user;
            (void)password;
#endif
            break;
    }

    return SQL_SUCCESS;
}
