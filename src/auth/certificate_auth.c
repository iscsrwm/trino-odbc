/* Certificate authentication implementation */

#include "trino_odbc.h"
#include "trino_odbc/auth.h"
#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

/* Configure curl for mutual TLS authentication */
SQLRETURN trino_cert_auth_configure(CURL *handle,
                                    const char *cert_path,
                                    const char *key_path,
                                    const char *key_password,
                                    const char *ca_path)
{
    if (!handle) return SQL_ERROR;

    if (cert_path) {
        curl_easy_setopt(handle, CURLOPT_SSLCERT, cert_path);
        curl_easy_setopt(handle, CURLOPT_SSLCERTTYPE, "PEM");
    }

    if (key_path) {
        curl_easy_setopt(handle, CURLOPT_SSLKEY, key_path);
        curl_easy_setopt(handle, CURLOPT_SSLKEYTYPE, "PEM");
    }

    if (key_password) {
        curl_easy_setopt(handle, CURLOPT_KEYPASSWD, key_password);
    }

    if (ca_path) {
        curl_easy_setopt(handle, CURLOPT_CAINFO, ca_path);
    }

    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(handle, CURLOPT_SSL_VERIFYHOST, 2L);

    return SQL_SUCCESS;
}
