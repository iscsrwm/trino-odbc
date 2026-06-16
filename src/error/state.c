#include "trino_odbc/error.h"
#include <string.h>
#include <stdio.h>

/* SQLState lookup helpers */

bool trino_is_success(const char *sqlstate)
{
    return sqlstate[0] == '0' || sqlstate[0] == '1' || sqlstate[0] == '2';
}

bool trino_is_warning(const char *sqlstate)
{
    return sqlstate[0] == '0' && sqlstate[1] == '1';
}

/* Map HTTP status codes to SQLState */
const char *trino_http_to_sqlstate(int http_status)
{
    switch (http_status) {
        case 200: return TRINO_SQLSTATE_SUCCESS;
        case 400: return TRINO_SQLSTATE_SYNTAX_ERROR;
        case 401: return TRINO_SQLSTATE_LOGIN_FAILED;
        case 403: return TRINO_SQLSTATE_PERMISSION_DENIED;
        case 404: return TRINO_SQLSTATE_REQUEST_FAILED;
        case 408: return TRINO_SQLSTATE_TIMEOUT;
        case 500: return TRINO_SQLSTATE_QUERY_FAILED;
        case 502: return TRINO_SQLSTATE_PROTOCOL_ERROR;
        case 503: return TRINO_SQLSTATE_PROTOCOL_ERROR;
        case 504: return TRINO_SQLSTATE_TIMEOUT;
        default:  return TRINO_SQLSTATE_REQUEST_FAILED;
    }
}

/* Format error message with context */
void trino_format_error(SQLCHAR *buffer, SQLINTEGER buffer_length,
                        const char *sqlstate, const char *context, const char *detail)
{
    if (!buffer || buffer_length == 0) return;

    if (detail) {
        snprintf((char *)buffer, (size_t)buffer_length,
                 "[SQLState=%s] %s: %s", sqlstate, context, detail);
    } else if (context) {
        snprintf((char *)buffer, (size_t)buffer_length,
                 "[SQLState=%s] %s", sqlstate, context);
    } else {
        snprintf((char *)buffer, (size_t)buffer_length,
                 "[SQLState=%s] Unknown error", sqlstate);
    }
    buffer[buffer_length - 1] = '\0';
}
