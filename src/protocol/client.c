#include "trino_odbc/protocol.h"
#include "trino_odbc/auth.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ========================================================================
 * HTTP Client lifecycle
 * ======================================================================== */

trino_http_client_t *trino_http_client_create(void)
{
    trino_http_client_t *client = calloc(1, sizeof(*client));
    if (!client)
        return NULL;

    client->easy_handle = curl_easy_init();
    if (!client->easy_handle) {
        free(client);
        return NULL;
    }

    /* Attach to the process-wide connection pool so this handle reuses pooled
     * TCP/TLS connections and the shared DNS cache. */
    client->share_handle = trino_http_pool_acquire();
    if (client->share_handle) {
        curl_easy_setopt(client->easy_handle, CURLOPT_SHARE, client->share_handle);
    }

    client->connect_timeout = 30;
    client->request_timeout = 300;
    return client;
}

void trino_http_client_destroy(trino_http_client_t *client)
{
    if (!client)
        return;

    if (client->easy_handle) {
        /* Detach from the share handle before cleaning up the easy handle, so
         * the pool is not referenced by a half-destroyed handle. */
        curl_easy_setopt(client->easy_handle, CURLOPT_SHARE, NULL);
        curl_easy_cleanup(client->easy_handle);
    }
    if (client->share_handle) {
        trino_http_pool_release();
        client->share_handle = NULL;
    }

    free(client->server_url);
    free(client->user);
    free(client->password);
    free(client->auth_type);
    free(client->ssl_truststore);
    free(client->client_tags_json);
    free(client->session_properties_json);
    free(client->source);
    free(client);
}

SQLRETURN trino_http_client_configure(
    trino_http_client_t *client, const char *server, SQLINTEGER port, const char *user,
    const char *password, const char *auth_type, bool ssl, const char *ssl_truststore,
    const char *client_tags_json, const char *session_properties_json, const char *source)
{
    if (!client || !server)
        return SQL_ERROR;

    /* Build server URL */
    char url_buf[512];
    const char *scheme = ssl ? "https" : "http";
    snprintf(url_buf, sizeof(url_buf), "%s://%s:%d", scheme, server, port);

    free(client->server_url);
    client->server_url = strdup(url_buf);

    free(client->user);
    client->user = user ? strdup(user) : NULL;

    free(client->password);
    client->password = password ? strdup(password) : NULL;

    free(client->auth_type);
    client->auth_type = auth_type ? strdup(auth_type) : strdup("NONE");

    client->ssl_enabled = ssl;

    free(client->ssl_truststore);
    client->ssl_truststore = ssl_truststore ? strdup(ssl_truststore) : NULL;

    free(client->client_tags_json);
    client->client_tags_json = client_tags_json ? strdup(client_tags_json) : NULL;

    free(client->session_properties_json);
    client->session_properties_json =
        session_properties_json ? strdup(session_properties_json) : NULL;

    free(client->source);
    client->source = source ? strdup(source) : strdup("trino-odbc");

    /* Configure curl defaults */
    curl_easy_setopt(client->easy_handle, CURLOPT_TIMEOUT, (long)client->request_timeout);
    curl_easy_setopt(client->easy_handle, CURLOPT_CONNECTTIMEOUT,
                     (long)client->connect_timeout);
    curl_easy_setopt(client->easy_handle, CURLOPT_FOLLOWLOCATION, 1L);

    if (ssl) {
        if (ssl_truststore) {
            curl_easy_setopt(client->easy_handle, CURLOPT_CAINFO, ssl_truststore);
        }
        /* Allow verification but permit self-signed in dev */
        curl_easy_setopt(client->easy_handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(client->easy_handle, CURLOPT_SSL_VERIFYHOST, 2L);
    } else {
        curl_easy_setopt(client->easy_handle, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(client->easy_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    return SQL_SUCCESS;
}

/* ========================================================================
 * Memory chunk for curl response callback
 * ======================================================================== */

typedef struct {
    char *mem;
    size_t size;
} memchunk_t;

static size_t memchunk_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    memchunk_t *chunk = (memchunk_t *)userp;

    char *ptr = realloc(chunk->mem, chunk->size + realsize + 1);
    if (!ptr)
        return 0;

    chunk->mem = ptr;
    memcpy(&chunk->mem[chunk->size], contents, realsize);
    chunk->size += realsize;
    chunk->mem[chunk->size] = '\0';

    return realsize;
}

/* ========================================================================
 * Transport abstraction (real libcurl, or a test hook)
 * ======================================================================== */

static trino_http_transport_fn g_test_transport = NULL;
static void *g_test_transport_ctx = NULL;

/* Sanitize a user-controlled HTTP header value into `out` (NUL-terminated).
 * CR and LF characters are dropped to prevent header/request injection, and
 * the result is truncated to out_size-1 bytes. Safe for out_size >= 1. */
void trino_http_sanitize_header_value(const char *value, char *out, size_t out_size)
{
    if (!out || out_size == 0)
        return;
    size_t pos = 0;
    if (value) {
        for (const char *v = value; *v && pos < out_size - 1; v++) {
            unsigned char c = (unsigned char)*v;
            if (c == '\r' || c == '\n')
                continue; /* strip injection chars */
            out[pos++] = (char)c;
        }
    }
    out[pos] = '\0';
}

/* Append an HTTP header "<name>: <value>" to the curl header list, sanitizing
 * the value to prevent header injection (see trino_http_sanitize_header_value).
 * If the value is NULL/empty, the list is returned unchanged. */
static struct curl_slist *append_safe_header(struct curl_slist *headers, const char *name,
                                             const char *value)
{
    if (!value || !*value)
        return headers;

    char buf[2048];
    size_t pos = 0;
    for (const char *n = name; *n && pos < sizeof(buf) - 3; n++) {
        buf[pos++] = *n;
    }
    buf[pos++] = ':';
    buf[pos++] = ' ';

    char sanitized[2000];
    trino_http_sanitize_header_value(value, sanitized, sizeof(sanitized));
    for (const char *v = sanitized; *v && pos < sizeof(buf) - 1; v++) {
        buf[pos++] = *v;
    }
    buf[pos] = '\0';

    return curl_slist_append(headers, buf);
}

void trino_http_set_test_transport(trino_http_transport_fn fn, void *user_ctx)
{
    g_test_transport = fn;
    g_test_transport_ctx = user_ctx;
}

/* Perform a single HTTP request, returning the response body as a freshly
 * allocated NUL-terminated string (caller frees), or NULL on failure.
 * `headers` is consumed (freed) by this function when libcurl is used. */
static char *perform_request(trino_http_client_t *client, const char *method,
                             const char *url, const char *body,
                             struct curl_slist *headers)
{
    if (g_test_transport) {
        if (headers)
            curl_slist_free_all(headers);
        return g_test_transport(method, url, body, g_test_transport_ctx);
    }

    if (strcmp(method, "GET") == 0) {
        curl_easy_setopt(client->easy_handle, CURLOPT_URL, url);
        curl_easy_setopt(client->easy_handle, CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(client->easy_handle, CURLOPT_POST, 0L);
    } else {
        curl_easy_setopt(client->easy_handle, CURLOPT_URL, url);
        curl_easy_setopt(client->easy_handle, CURLOPT_POST, 1L);
        curl_easy_setopt(client->easy_handle, CURLOPT_POSTFIELDS, body ? body : "");
    }

    /* Always set the header list (possibly NULL). The easy handle is reused
     * across requests, so failing to reset this would leave it pointing at a
     * previous request's header list, which is freed below — causing a
     * use-after-free inside libcurl on the next perform. */
    curl_easy_setopt(client->easy_handle, CURLOPT_HTTPHEADER, headers);

    memchunk_t chunk = {0};
    chunk.mem = calloc(1, 1);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEFUNCTION, memchunk_callback);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEDATA, &chunk);

    CURLcode res = curl_easy_perform(client->easy_handle);

    /* Detach the header list from the handle before freeing it, so a later
     * perform on the reused handle cannot reference freed memory. */
    curl_easy_setopt(client->easy_handle, CURLOPT_HTTPHEADER, NULL);
    if (headers)
        curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        free(chunk.mem);
        return NULL;
    }
    return chunk.mem; /* caller frees */
}

/* ========================================================================
 * Execute SQL query
 * ======================================================================== */

trino_query_results_t *trino_http_client_query(trino_http_client_t *client,
                                               const SQLCHAR *sql, SQLRETURN *retcode)
{
    if (!client || !sql) {
        if (retcode)
            *retcode = SQL_ERROR;
        return NULL;
    }
    /* easy_handle is required only for the real transport. */
    if (!g_test_transport && !client->easy_handle) {
        if (retcode)
            *retcode = SQL_ERROR;
        return NULL;
    }

    /* Build URL */
    char url[1024];
    snprintf(url, sizeof(url), "%s/v1/statement", client->server_url);

    /* Apply authentication (real transport only) */
    if (!g_test_transport) {
        trino_auth_method_t auth = trino_auth_parse(client->auth_type);
        SQLRETURN auth_ret = trino_auth_apply(client->easy_handle, auth, client->user,
                                              client->password, client->ssl_truststore);
        if (auth_ret != SQL_SUCCESS) {
            if (retcode)
                *retcode = SQL_ERROR;
            return NULL;
        }
    }

    /* Build request headers. User-controlled values (source, client tags) are
     * sanitized to prevent CRLF header injection. */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: text/plain");
    headers = append_safe_header(headers, "X-Trino-Client-Tags",
                                 (const char *)client->client_tags_json);
    headers = append_safe_header(headers, "X-Trino-Source", (const char *)client->source);

    char *response = perform_request(client, "POST", url, (const char *)sql, headers);
    if (!response) {
        if (retcode)
            *retcode = SQL_ERROR;
        return NULL;
    }

    /* Parse response */
    trino_query_results_t *results = calloc(1, sizeof(*results));
    if (!results) {
        free(response);
        if (retcode)
            *retcode = SQL_ERROR;
        return NULL;
    }

    SQLRETURN parse_ret = trino_parse_query_response(response, results);
    free(response);

    if (parse_ret == SQL_ERROR && results->has_error) {
        /* Trino reported a query error; surface it to the caller. */
        if (retcode)
            *retcode = SQL_ERROR;
        return results;
    }
    if (parse_ret == SQL_ERROR) {
        /* Malformed response. */
        trino_query_results_free(results);
        if (retcode)
            *retcode = SQL_ERROR;
        return NULL;
    }

    /* The initial POST response typically carries only an id and nextUri; the
     * column metadata and first rows arrive on subsequent GET pages. Follow the
     * nextUri chain until columns are known and at least one data page has been
     * consumed, the query finishes, or there are no further pages. */
    while (results->next_uri && (results->columns == NULL || results->row_count == 0) &&
           results->state != TRINO_QUERY_STATE_FINISHED &&
           results->state != TRINO_QUERY_STATE_FAILED &&
           results->state != TRINO_QUERY_STATE_CANCELLED) {
        SQLRETURN fr = trino_http_client_fetch_next(client, results);
        if (fr == SQL_ERROR) {
            if (results->has_error) {
                if (retcode)
                    *retcode = SQL_ERROR;
                return results;
            }
            trino_query_results_free(results);
            if (retcode)
                *retcode = SQL_ERROR;
            return NULL;
        }
        if (fr == SQL_NO_DATA) {
            break;
        }
    }

    if (retcode)
        *retcode = SQL_SUCCESS;
    return results;
}

/* ========================================================================
 * Fetch next batch
 * ======================================================================== */

SQLRETURN trino_http_client_fetch_next(trino_http_client_t *client,
                                       trino_query_results_t *results)
{
    if (!client || !results || !results->next_uri) {
        return SQL_NO_DATA;
    }

    /* Apply authentication (real transport only) */
    if (!g_test_transport) {
        trino_auth_method_t auth = trino_auth_parse(client->auth_type);
        trino_auth_apply(client->easy_handle, auth, client->user, client->password,
                         client->ssl_truststore);
    }

    char *response =
        perform_request(client, "GET", (const char *)results->next_uri, NULL, NULL);
    if (!response) {
        return SQL_ERROR;
    }

    /* trino_parse_query_response updates nextUri/state/stats, parses columns if
     * not yet known, and appends this page's data rows to results->rows. */
    SQLRETURN parse_ret = trino_parse_query_response(response, results);
    free(response);

    return parse_ret;
}

/* ========================================================================
 * Kill query
 * ======================================================================== */

SQLRETURN trino_http_client_kill_query(trino_http_client_t *client, const char *query_id)
{
    if (!client || !query_id)
        return SQL_ERROR;

    char url[1024];
    snprintf(url, sizeof(url), "%s/v1/query/%s/kill", client->server_url, query_id);

    trino_auth_method_t auth = trino_auth_parse(client->auth_type);
    trino_auth_apply(client->easy_handle, auth, client->user, client->password,
                     client->ssl_truststore);

    curl_easy_setopt(client->easy_handle, CURLOPT_URL, url);
    curl_easy_setopt(client->easy_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(client->easy_handle, CURLOPT_POSTFIELDS, "");

    memchunk_t chunk = {0};
    chunk.mem = calloc(1, 1);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEFUNCTION, memchunk_callback);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEDATA, &chunk);

    CURLcode res = curl_easy_perform(client->easy_handle);
    free(chunk.mem);

    return (res == CURLE_OK) ? SQL_SUCCESS : SQL_ERROR;
}
