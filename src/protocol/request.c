/* Request builder for Trino protocol */

#include "trino_odbc/protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Build the full request URL with session properties */
char *trino_build_request_url(const char *base_url, const char *session_properties)
{
    if (!session_properties || strlen(session_properties) == 0) {
        return strdup(base_url);
    }

    char *url = malloc(strlen(base_url) + strlen(session_properties) + 32);
    if (!url) return NULL;

    snprintf(url, strlen(base_url) + strlen(session_properties) + 32,
             "%s?sessionProperties=%s", base_url, session_properties);
    return url;
}

/* Format client tags as JSON array if not already */
char *trino_format_client_tags(const char *tags)
{
    if (!tags) return NULL;

    /* If already JSON (starts with [), return as-is */
    if (tags[0] == '[') {
        return strdup(tags);
    }

    /* Wrap in JSON array */
    char *result = malloc(strlen(tags) + 32);
    if (!result) return NULL;

    snprintf(result, strlen(tags) + 32, "[\"%s\"]", tags);
    return result;
}
