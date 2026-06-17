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

/* ------------------------------------------------------------------------
 * Threading: the driver uses a small subset of the pthreads mutex API. On
 * Windows there is no <pthread.h>; map that subset onto an SRWLOCK, which
 * (unlike CRITICAL_SECTION) supports a static initializer for file-scope
 * mutexes declared with PTHREAD_MUTEX_INITIALIZER.
 * ------------------------------------------------------------------------ */
#include <windows.h>

typedef SRWLOCK pthread_mutex_t;

#define PTHREAD_MUTEX_INITIALIZER SRWLOCK_INIT

static inline int pthread_mutex_init(pthread_mutex_t *m, const void *attr)
{
    (void)attr;
    InitializeSRWLock(m);
    return 0;
}

static inline int pthread_mutex_destroy(pthread_mutex_t *m)
{
    (void)m; /* SRWLOCKs require no teardown. */
    return 0;
}

static inline int pthread_mutex_lock(pthread_mutex_t *m)
{
    AcquireSRWLockExclusive(m);
    return 0;
}

static inline int pthread_mutex_unlock(pthread_mutex_t *m)
{
    ReleaseSRWLockExclusive(m);
    return 0;
}

#endif /* _WIN32 */

#endif /* TRINO_ODBC_COMPAT_H */
