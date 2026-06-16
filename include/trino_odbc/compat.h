#ifndef TRINO_ODBC_COMPAT_H
#define TRINO_ODBC_COMPAT_H

/*
 * Small portability shim for POSIX functions used by the driver that MSVC /
 * the Windows CRT spell differently or do not provide. Keeps the rest of the
 * codebase free of per-call-site #ifdefs.
 */

#include <string.h>
#include <stdlib.h>

#ifdef _WIN32

/* Case-insensitive compares. */
#ifndef strcasecmp
#define strcasecmp _stricmp
#endif
#ifndef strncasecmp
#define strncasecmp _strnicmp
#endif

/* Reentrant tokenizer: MSVC spells it strtok_s with the same signature. */
#ifndef strtok_r
#define strtok_r strtok_s
#endif

/* strndup is not provided by the Windows CRT. */
#include <stddef.h>
static inline char *trino_strndup(const char *s, size_t n)
{
    size_t len = 0;
    while (len < n && s[len] != '\0')
        len++;
    char *out = (char *)malloc(len + 1);
    if (!out)
        return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
#ifndef strndup
#define strndup trino_strndup
#endif

#endif /* _WIN32 */

#endif /* TRINO_ODBC_COMPAT_H */
