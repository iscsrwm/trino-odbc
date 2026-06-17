/* Connection pool: a process-wide curl share handle that pools the underlying
 * TCP/TLS connections, DNS cache, and TLS sessions across all driver
 * connections. HTTP easy handles attach to it via CURLOPT_SHARE so that
 * repeated statements/queries reuse sockets instead of reconnecting.
 *
 * curl share handles are not internally locked, so we provide lock/unlock
 * callbacks. The pool is reference-counted: created on first acquire and torn
 * down on the last release.
 */

#include "trino_odbc/protocol.h"
#ifdef _WIN32
#include "trino_odbc/compat.h"
#else
#include <pthread.h>
#endif
#include <stdlib.h>

/* One mutex per shareable data type curl may lock concurrently. */
static pthread_mutex_t g_share_locks[CURL_LOCK_DATA_LAST];
static pthread_mutex_t g_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

static CURLSH *g_share = NULL;
static int g_refcount = 0;
static bool g_locks_inited = false;
static bool g_curl_global_inited = false;

static void share_lock_cb(CURL *handle, curl_lock_data data, curl_lock_access access,
                          void *userptr)
{
    (void)handle;
    (void)access;
    (void)userptr;
    if (data < CURL_LOCK_DATA_LAST)
        pthread_mutex_lock(&g_share_locks[data]);
}

static void share_unlock_cb(CURL *handle, curl_lock_data data, void *userptr)
{
    (void)handle;
    (void)userptr;
    if (data < CURL_LOCK_DATA_LAST)
        pthread_mutex_unlock(&g_share_locks[data]);
}

/* Initialize libcurl's global state exactly once before any easy/share handle
 * is created. This MUST happen explicitly: libcurl's implicit lazy init is not
 * thread-safe and, on Windows with the schannel TLS backend, performing an
 * HTTPS request without a prior curl_global_init() crashes (access violation)
 * during TLS setup. */
SQLRETURN trino_curl_global_ensure_init(void)
{
    SQLRETURN ret = SQL_SUCCESS;
    pthread_mutex_lock(&g_pool_mutex);
    if (!g_curl_global_inited) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) {
            g_curl_global_inited = true;
        } else {
            ret = SQL_ERROR;
        }
    }
    pthread_mutex_unlock(&g_pool_mutex);
    return ret;
}

CURLSH *trino_http_pool_acquire(void)
{
    /* Guarantee global init before any share/easy handle is created. */
    if (trino_curl_global_ensure_init() != SQL_SUCCESS)
        return NULL;

    pthread_mutex_lock(&g_pool_mutex);

    if (g_share == NULL) {
        if (!g_locks_inited) {
            for (int i = 0; i < CURL_LOCK_DATA_LAST; i++)
                pthread_mutex_init(&g_share_locks[i], NULL);
            g_locks_inited = true;
        }

        g_share = curl_share_init();
        if (!g_share) {
            pthread_mutex_unlock(&g_pool_mutex);
            return NULL;
        }
        curl_share_setopt(g_share, CURLSHOPT_LOCKFUNC, share_lock_cb);
        curl_share_setopt(g_share, CURLSHOPT_UNLOCKFUNC, share_unlock_cb);
        /* Pool connections, DNS results and TLS sessions across handles. */
        curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
        curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_DNS);
        curl_share_setopt(g_share, CURLSHOPT_SHARE, CURL_LOCK_DATA_SSL_SESSION);
    }

    g_refcount++;
    CURLSH *share = g_share;
    pthread_mutex_unlock(&g_pool_mutex);
    return share;
}

void trino_http_pool_release(void)
{
    pthread_mutex_lock(&g_pool_mutex);

    if (g_refcount > 0)
        g_refcount--;

    if (g_refcount == 0 && g_share != NULL) {
        curl_share_cleanup(g_share);
        g_share = NULL;
        /* Leave the lock mutexes initialized; they are reused if the pool is
         * acquired again, and destroying them here would race with any handle
         * still mid-callback. They are process-lifetime and cheap. */
    }

    pthread_mutex_unlock(&g_pool_mutex);
}
