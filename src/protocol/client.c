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
    if (!client) return NULL;

    client->easy_handle = curl_easy_init();
    if (!client->easy_handle) {
        free(client);
        return NULL;
    }

    client->connect_timeout = 30;
    client->request_timeout = 300;
    return client;
}

void trino_http_client_destroy(trino_http_client_t *client)
{
    if (!client) return;

    if (client->easy_handle) {
        curl_easy_cleanup(client->easy_handle);
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

SQLRETURN trino_http_client_configure(trino_http_client_t *client,
                                      const char *server, SQLINTEGER port,
                                      const char *user, const char *password,
                                      const char *auth_type, bool ssl,
                                      const char *ssl_truststore,
                                      const char *client_tags_json,
                                      const char *session_properties_json,
                                      const char *source)
{
    if (!client || !server) return SQL_ERROR;

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
    client->session_properties_json = session_properties_json ?
        strdup(session_properties_json) : NULL;

    free(client->source);
    client->source = source ? strdup(source) : strdup("trino-odbc");

    /* Configure curl defaults */
    curl_easy_setopt(client->easy_handle, CURLOPT_TIMEOUT, (long)client->request_timeout);
    curl_easy_setopt(client->easy_handle, CURLOPT_CONNECTTIMEOUT, (long)client->connect_timeout);
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
    if (!ptr) return 0;

    chunk->mem = ptr;
    memcpy(&chunk->mem[chunk->size], contents, realsize);
    chunk->size += realsize;
    chunk->mem[chunk->size] = '\0';

    return realsize;
}

/* ========================================================================
 * Simple JSON helpers (minimal parser for Trino response)
 * ======================================================================== */

/* Find a JSON string value for a key — returns pointer into the string or NULL */
static const char *json_find_string(const char *json, const char *key)
{
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return NULL;

    pos = strchr(pos + strlen(search), ':');
    if (!pos) return NULL;
    pos++;

    /* Skip whitespace */
    while (*pos && isspace((unsigned char)*pos)) pos++;

    if (*pos != '"') return NULL;
    pos++; /* skip opening quote */

    const char *start = pos;
    const char *end = strchr(pos, '"');
    if (!end) return NULL;

    /* Return the string (caller should copy it) */
    size_t len = (size_t)(end - start);
    char *result = malloc(len + 1);
    if (!result) return NULL;
    memcpy(result, start, len);
    result[len] = '\0';
    return (const char *)result;
}

static int json_find_int(const char *json, const char *key, int default_val)
{
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return default_val;

    pos = strchr(pos + strlen(search), ':');
    if (!pos) return default_val;
    pos++;

    while (*pos && isspace((unsigned char)*pos)) pos++;

    if (*pos == '"' || *pos == '{' || *pos == '[') return default_val;

    return atoi(pos);
}

static double json_find_double(const char *json, const char *key, double default_val)
{
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return default_val;

    pos = strchr(pos + strlen(search), ':');
    if (!pos) return default_val;
    pos++;

    while (*pos && isspace((unsigned char)*pos)) pos++;

    if (*pos == '"' || *pos == '{' || *pos == '[') return default_val;

    return atof(pos);
}

static bool json_has_key(const char *json, const char *key)
{
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);
    return strstr(json, search) != NULL;
}

/* ========================================================================
 * Execute SQL query
 * ======================================================================== */

trino_query_results_t *trino_http_client_query(trino_http_client_t *client,
                                               const SQLCHAR *sql,
                                               SQLRETURN *retcode)
{
    if (!client || !sql || !client->easy_handle) {
        if (retcode) *retcode = SQL_ERROR;
        return NULL;
    }

    /* Build URL */
    char url[1024];
    snprintf(url, sizeof(url), "%s/v1/statement", client->server_url);

    /* Apply authentication */
    trino_auth_method_t auth = trino_auth_parse(client->auth_type);
    SQLRETURN auth_ret = trino_auth_apply(client->easy_handle, auth,
                                          client->user, client->password,
                                          client->ssl_truststore);
    if (auth_ret != SQL_SUCCESS) {
        if (retcode) *retcode = SQL_ERROR;
        return NULL;
    }

    /* Set URL */
    curl_easy_setopt(client->easy_handle, CURLOPT_URL, url);

    /* Set POST body (SQL text) */
    curl_easy_setopt(client->easy_handle, CURLOPT_POST, 1L);
    curl_easy_setopt(client->easy_handle, CURLOPT_POSTFIELDS, (char *)sql);

    /* Set headers */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: text/plain");

    /* Add X-Trino-Client-Tags header if configured */
    if (client->client_tags_json) {
        char tag_header[2048];
        snprintf(tag_header, sizeof(tag_header), "X-Trino-Client-Tags: %s",
                 client->client_tags_json);
        headers = curl_slist_append(headers, tag_header);
    }

    /* Add X-Trino-Source header */
    if (client->source) {
        char source_header[512];
        snprintf(source_header, sizeof(source_header), "X-Trino-Source: %s",
                 client->source);
        headers = curl_slist_append(headers, source_header);
    }

    curl_easy_setopt(client->easy_handle, CURLOPT_HTTPHEADER, headers);

    /* Response capture */
    memchunk_t chunk = {0};
    chunk.mem = calloc(1, 1);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEFUNCTION, memchunk_callback);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEDATA, &chunk);

    /* Capture response headers for query ID */
    char response_header_buf[2048] = {0};
    size_t header_buf_len = 0;

    CURLcode res = curl_easy_perform(client->easy_handle);

    long http_code = 0;
    curl_easy_getinfo(client->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        if (retcode) *retcode = SQL_ERROR;
        free(chunk.mem);
        return NULL;
    }

    /* Parse response */
    trino_query_results_t *results = calloc(1, sizeof(*results));
    if (!results) {
        free(chunk.mem);
        if (retcode) *retcode = SQL_ERROR;
        return NULL;
    }

    const char *json = chunk.mem;

    /* Extract query ID */
    const char *qid = json_find_string(json, "id");
    if (qid) {
        strncpy((char *)results->query_id, (char *)qid, sizeof(results->query_id) - 1);
        free((void *)qid);
    }

    /* Extract nextUri */
    const char *nuri = json_find_string(json, "nextUri");
    if (nuri) {
        results->next_uri = strdup((char *)nuri);
        free((void *)nuri);
    }

    /* Check for error */
    if (json_has_key(json, "error")) {
        results->has_error = true;

        /* Find error object — simplified parsing */
        const char *err_pos = strstr(json, "\"error\"");
        if (err_pos) {
            const char *name = json_find_string(err_pos, "name");
            if (name) {
                results->error_name = strdup((char *)name);
                free((void *)name);
            }
            const char *msg = json_find_string(err_pos, "message");
            if (msg) {
                results->error_message = strdup((char *)msg);
                free((void *)msg);
            }
            const char *etype = json_find_string(err_pos, "errorType");
            if (etype) {
                results->error_type = strdup((char *)etype);
                free((void *)etype);
            }
        }

        free(chunk.mem);
        if (retcode) *retcode = SQL_ERROR;
        return results;
    }

    /* Parse state from stats */
    const char *state_str = json_find_string(json, "state");
    if (state_str) {
        if (strcmp((char *)state_str, "FINISHED") == 0) {
            results->state = TRINO_QUERY_STATE_FINISHED;
        } else if (strcmp((char *)state_str, "FAILED") == 0) {
            results->state = TRINO_QUERY_STATE_FAILED;
        } else if (strcmp((char *)state_str, "CANCELLED") == 0) {
            results->state = TRINO_QUERY_STATE_CANCELLED;
        } else {
            results->state = TRINO_QUERY_STATE_RUNNING;
        }
        free((void *)state_str);
    }

    /* Parse columns — we need a more sophisticated JSON parser for arrays.
     * For now, we'll use a simple approach. */
    /* TODO: Implement proper column parsing from JSON array */

    /* Parse data rows */
    /* TODO: Implement proper row parsing from JSON 2D array */

    /* Parse stats */
    if (json_has_key(json, "stats")) {
        const char *stats_pos = strstr(json, "\"stats\"");
        if (stats_pos) {
            results->rows_processed = (SQLULEN)json_find_int(stats_pos, "processedRows", 0);
            results->bytes_processed = (SQLULEN)json_find_int(stats_pos, "processedBytes", 0);
            results->elapsed_time = json_find_double(stats_pos, "elapsedTime", 0.0);
        }
    }

    free(chunk.mem);

    if (retcode) *retcode = SQL_SUCCESS;
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

    /* Clear previous results */
    /* TODO: Free previous row data */

    /* Apply authentication */
    trino_auth_method_t auth = trino_auth_parse(client->auth_type);
    trino_auth_apply(client->easy_handle, auth,
                     client->user, client->password,
                     client->ssl_truststore);

    /* GET nextUri */
    curl_easy_setopt(client->easy_handle, CURLOPT_URL, results->next_uri);
    curl_easy_setopt(client->easy_handle, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(client->easy_handle, CURLOPT_POST, 0L);

    memchunk_t chunk = {0};
    chunk.mem = calloc(1, 1);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEFUNCTION, memchunk_callback);
    curl_easy_setopt(client->easy_handle, CURLOPT_WRITEDATA, &chunk);

    CURLcode res = curl_easy_perform(client->easy_handle);

    long http_code = 0;
    curl_easy_getinfo(client->easy_handle, CURLINFO_RESPONSE_CODE, &http_code);

    if (res != CURLE_OK) {
        free(chunk.mem);
        return SQL_ERROR;
    }

    /* Parse response — same structure as initial query response */
    const char *json = chunk.mem;

    /* Update nextUri */
    free(results->next_uri);
    results->next_uri = NULL;
    const char *nuri = json_find_string(json, "nextUri");
    if (nuri) {
        results->next_uri = strdup((char *)nuri);
        free((void *)nuri);
    }

    /* Check for error */
    if (json_has_key(json, "error")) {
        results->has_error = true;
        const char *err_pos = strstr(json, "\"error\"");
        if (err_pos) {
            const char *name = json_find_string(err_pos, "name");
            if (name) {
                results->error_name = strdup((char *)name);
                free((void *)name);
            }
            const char *msg = json_find_string(err_pos, "message");
            if (msg) {
                results->error_message = strdup((char *)msg);
                free((void *)msg);
            }
        }
        free(chunk.mem);
        return SQL_ERROR;
    }

    /* Update state */
    const char *state_str = json_find_string(json, "state");
    if (state_str) {
        if (strcmp((char *)state_str, "FINISHED") == 0) {
            results->state = TRINO_QUERY_STATE_FINISHED;
        } else if (strcmp((char *)state_str, "FAILED") == 0) {
            results->state = TRINO_QUERY_STATE_FAILED;
        }
        free((void *)state_str);
    }

    /* TODO: Parse new rows from data array */

    free(chunk.mem);
    return SQL_SUCCESS;
}

/* ========================================================================
 * Kill query
 * ======================================================================== */

SQLRETURN trino_http_client_kill_query(trino_http_client_t *client,
                                       const SQLCHAR *query_id)
{
    if (!client || !query_id) return SQL_ERROR;

    char url[1024];
    snprintf(url, sizeof(url), "%s/v1/query/%s/kill",
             client->server_url, query_id);

    trino_auth_method_t auth = trino_auth_parse(client->auth_type);
    trino_auth_apply(client->easy_handle, auth,
                     client->user, client->password,
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

/* ========================================================================
 * Free results
 * ======================================================================== */

void trino_query_results_free(trino_query_results_t *results)
{
    if (!results) return;

    free(results->next_uri);
    free(results->error_name);
    free(results->error_message);
    free(results->error_type);
    free(results->error_uri);
    free(results->stats_uri);
    free(results->task_info_uri);

    /* Free column metadata */
    free(results->columns);

    /* Free row data */
    if (results->rows) {
        for (SQLULEN i = 0; i < results->row_count; i++) {
            free(results->rows[i]);
        }
        free(results->rows);
    }

    free(results);
}

/* ========================================================================
 * Type mapping
 * ======================================================================== */

SQLSMALLINT trino_type_to_odbc_type(const char *trino_type)
{
    if (!trino_type) return SQL_VARCHAR;

    /* Compare base type (before <...> for parameterized types) */
    if (strncmp(trino_type, "varchar", 7) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "char", 4) == 0) return SQL_CHAR;
    if (strncmp(trino_type, "varbinary", 9) == 0) return SQL_VARBINARY;
    if (strncmp(trino_type, "boolean", 7) == 0) return SQL_BIT;
    if (strncmp(trino_type, "tinyint", 7) == 0) return SQL_TINYINT;
    if (strncmp(trino_type, "smallint", 8) == 0) return SQL_SMALLINT;
    if (strncmp(trino_type, "integer", 7) == 0) return SQL_INTEGER;
    if (strncmp(trino_type, "bigint", 6) == 0) return SQL_BIGINT;
    if (strncmp(trino_type, "real", 4) == 0) return SQL_REAL;
    if (strncmp(trino_type, "double", 6) == 0) return SQL_DOUBLE;
    if (strncmp(trino_type, "decimal", 7) == 0) return SQL_DECIMAL;
    if (strncmp(trino_type, "date", 4) == 0) return SQL_TYPE_DATE;
    if (strncmp(trino_type, "timestamp", 9) == 0) return SQL_TYPE_TIMESTAMP;
    if (strncmp(trino_type, "time", 4) == 0) return SQL_TYPE_TIME;
    if (strncmp(trino_type, "json", 4) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "array", 5) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "map", 3) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "row", 3) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "hyperloglog", 11) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "jitterbug", 9) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "qdq", 3) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "interval year to month", 22) == 0) return SQL_INTERVAL_YEAR_TO_MONTH;
    if (strncmp(trino_type, "interval day to second", 22) == 0) return SQL_INTERVAL_DAY_TO_SECOND;
    if (strncmp(trino_type, "ipaddress", 9) == 0) return SQL_VARCHAR;
    if (strncmp(trino_type, "uuid", 4) == 0) return SQL_GUID;

    return SQL_VARCHAR; /* default fallback */
}

const char *trino_type_name(SQLSMALLINT odbc_type)
{
    switch (odbc_type) {
        case SQL_VARCHAR:      return "VARCHAR";
        case SQL_CHAR:         return "CHAR";
        case SQL_VARBINARY:    return "VARBINARY";
        case SQL_BIT:          return "BOOLEAN";
        case SQL_TINYINT:      return "TINYINT";
        case SQL_SMALLINT:     return "SMALLINT";
        case SQL_INTEGER:      return "INTEGER";
        case SQL_BIGINT:       return "BIGINT";
        case SQL_REAL:         return "REAL";
        case SQL_DOUBLE:       return "DOUBLE";
        case SQL_DECIMAL:      return "DECIMAL";
        case SQL_TYPE_DATE:    return "DATE";
        case SQL_TYPE_TIME:    return "TIME";
        case SQL_TYPE_TIMESTAMP: return "TIMESTAMP";
        case SQL_GUID:         return "UUID";
        default:              return "VARCHAR";
    }
}
