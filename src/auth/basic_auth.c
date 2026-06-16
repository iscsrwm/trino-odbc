/* Basic authentication implementation */

#include <curl/curl.h>
#include <stdlib.h>
#include <string.h>

/* Apply basic auth to curl handle — called from auth.c via trino_auth_apply */
/* This is a helper that formats the credentials */

char *trino_basic_auth_format(const char *user, const char *password)
{
    if (!user)
        return NULL;

    size_t len = strlen(user);
    if (password)
        len += 1 + strlen(password);

    char *result = malloc(len + 1);
    if (!result)
        return NULL;

    snprintf(result, len + 1, "%s:%s", user, password ? password : "");
    return result;
}
