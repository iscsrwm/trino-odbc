/* Response parsing utilities for Trino protocol using json-c */

#include "trino_odbc/protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int trino_type_to_odbc_type_from_json(const char *trino_type);

/* Parse column metadata from a JSON columns array using json-c */
trino_column_meta_t *trino_parse_columns_jsonc(json_object *columns_array, SQLULEN *column_count)
{
    if (!columns_array || !column_count) {
        return NULL;
    }

    if (!json_object_is_type(columns_array, json_type_array)) {
        *column_count = 0;
        return NULL;
    }

    size_t array_len = json_object_array_length(columns_array);
    if (array_len == 0) {
        *column_count = 0;
        return NULL;
    }

    trino_column_meta_t *columns = calloc(array_len, sizeof(trino_column_meta_t));
    if (!columns) {
        *column_count = 0;
        return NULL;
    }

    for (size_t i = 0; i < array_len; i++) {
        json_object *col_obj = json_object_array_get_idx(columns_array, i);
        
        /* Initialize defaults even if parsing fails */
        columns[i].odbc_type = SQL_VARCHAR;
        columns[i].nullable = 1;
        columns[i].name[0] = '\0';
        columns[i].type[0] = '\0';

        if (!json_object_is_type(col_obj, json_type_object)) {
            continue;
        }

        /* Extract name */
        json_object *name_obj = NULL;
        if (json_object_object_get_ex(col_obj, "name", &name_obj) && 
            json_object_is_type(name_obj, json_type_string)) {
            const char *name_str = json_object_get_string(name_obj);
            strncpy((char *)columns[i].name, name_str, TRINO_MAX_IDENTIFIER_LEN);
            columns[i].name[TRINO_MAX_IDENTIFIER_LEN] = '\0';
        }

        /* Extract type */
        json_object *type_obj = NULL;
        if (json_object_object_get_ex(col_obj, "type", &type_obj) && 
            json_object_is_type(type_obj, json_type_string)) {
            const char *type_str = json_object_get_string(type_obj);
            strncpy((char *)columns[i].type, type_str, TRINO_MAX_TYPE_NAME - 1);
            columns[i].type[TRINO_MAX_TYPE_NAME - 1] = '\0';
            columns[i].odbc_type = trino_type_to_odbc_type_from_json(type_str);
        } else {
            strncpy((char *)columns[i].type, "varchar", TRINO_MAX_TYPE_NAME - 1);
            columns[i].type[TRINO_MAX_TYPE_NAME - 1] = '\0';
            columns[i].odbc_type = SQL_VARCHAR;
        }

        /* Nullable defaults to true (SQL_NULLABLE) */
        columns[i].nullable = 1;
    }

    *column_count = (SQLULEN)array_len;
    return columns;
}

/* Wrapper for backward compatibility - parses full QueryResults JSON and extracts columns */
trino_column_meta_t *trino_parse_columns(const char *json, SQLULEN *column_count)
{
    if (!json || !column_count) {
        return NULL;
    }

    json_object *root = NULL;
    trino_column_meta_t *columns = NULL;

    root = json_tokener_parse(json);
    
#ifdef DEBUG
    fprintf(stderr, "DEBUG: trino_parse_columns parsed JSON, root=%p\n", (void*)root);
#endif

    if (!root || !json_object_is_type(root, json_type_object)) {
        goto cleanup;
    }

    /* Extract columns array */
    json_object *columns_obj = NULL;
    if (!json_object_object_get_ex(root, "columns", &columns_obj)) {
        *column_count = 0;
        goto cleanup;
    }

    columns = trino_parse_columns_jsonc(columns_obj, column_count);
    
#ifdef DEBUG
    fprintf(stderr, "DEBUG: trino_parse_columns returned %p with count=%lu\n", (void*)columns, *column_count);
#endif

cleanup:
    if (root) {
        json_object_put(root);
    }
    return columns;
}

/* Free query results */
void trino_query_results_free(trino_query_results_t *results)
{
    if (!results) return;

    /* Free strings */
    free(results->next_uri);
    free(results->error_name);
    free(results->error_message);
    free(results->error_type);
    free(results->error_uri);
    free(results->stats_uri);
    free(results->task_info_uri);

    /* Free columns */
    if (results->columns) {
        free(results->columns);
    }

    /* Free rows data */
    if (results->rows && results->row_count > 0) {
        for (SQLULEN i = 0; i < results->row_count; i++) {
            if (results->rows[i]) {
                for (SQLULEN j = 0; j < results->column_count; j++) {
                    free(results->rows[i][j]);
                }
                free(results->rows[i]);
            }
        }
        free(results->rows);
    }

    free(results);
}

/* Free only the row data, resetting the row count/capacity. Columns and other
 * metadata are left intact (used between pages of a streamed result set). */
void trino_query_results_free_rows(trino_query_results_t *results)
{
    if (!results || !results->rows) return;

    for (SQLULEN i = 0; i < results->row_count; i++) {
        if (results->rows[i]) {
            for (SQLULEN j = 0; j < results->column_count; j++) {
                free(results->rows[i][j]);
            }
            free(results->rows[i]);
        }
    }
    free(results->rows);
    results->rows = NULL;
    results->row_count = 0;
    results->row_capacity = 0;
}

/* Convert a single JSON cell value to a freshly-allocated NUL-terminated
 * string. NULL JSON values are represented as a NULL pointer (SQL NULL).
 * Nested arrays/objects are serialized back to their JSON text form so that
 * complex Trino types (array/map/row/json) surface as VARCHAR. */
static SQLCHAR *cell_to_string(json_object *cell)
{
    if (!cell || json_object_is_type(cell, json_type_null)) {
        return NULL; /* SQL NULL */
    }

    const char *text = NULL;
    if (json_object_is_type(cell, json_type_string)) {
        text = json_object_get_string(cell);
    } else if (json_object_is_type(cell, json_type_array) ||
               json_object_is_type(cell, json_type_object)) {
        text = json_object_to_json_string_ext(cell, JSON_C_TO_STRING_PLAIN);
    } else {
        /* int / double / boolean */
        text = json_object_get_string(cell);
    }

    if (!text) return NULL;

    size_t len = strlen(text);
    SQLCHAR *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, text, len + 1);
    return out;
}

/* Append the rows contained in a Trino "data" array to results->rows.
 * Each element of data_array is itself an array with column_count cells.
 * Returns 0 on success, -1 on allocation failure. */
static int append_data_rows(trino_query_results_t *results, json_object *data_array)
{
    if (!data_array || !json_object_is_type(data_array, json_type_array)) {
        return 0; /* no data on this page */
    }

    size_t new_rows = json_object_array_length(data_array);
    if (new_rows == 0) return 0;

    SQLULEN cols = results->column_count;

    /* Grow the row pointer array to hold the additional rows. */
    SQLULEN needed = results->row_count + (SQLULEN)new_rows;
    SQLCHAR ***grown = realloc(results->rows, needed * sizeof(*grown));
    if (!grown) return -1;
    results->rows = grown;
    results->row_capacity = needed;

    for (size_t r = 0; r < new_rows; r++) {
        json_object *row_arr = json_object_array_get_idx(data_array, r);
        SQLCHAR **cells = calloc(cols ? cols : 1, sizeof(*cells));
        if (!cells) return -1;

        if (json_object_is_type(row_arr, json_type_array)) {
            size_t avail = json_object_array_length(row_arr);
            for (SQLULEN c = 0; c < cols; c++) {
                if (c < avail) {
                    cells[c] = cell_to_string(json_object_array_get_idx(row_arr, c));
                } else {
                    cells[c] = NULL;
                }
            }
        }

        results->rows[results->row_count++] = cells;
    }

    return 0;
}

/* Parse a full Trino QueryResults JSON document. See protocol.h for contract. */
SQLRETURN trino_parse_query_response(const char *json_text,
                                     trino_query_results_t *results)
{
    if (!json_text || !results) return SQL_ERROR;

    json_object *root = json_tokener_parse(json_text);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return SQL_ERROR;
    }

    json_object *field = NULL;

    /* Query id */
    if (json_object_object_get_ex(root, "id", &field) &&
        json_object_is_type(field, json_type_string)) {
        const char *id = json_object_get_string(field);
        strncpy((char *)results->query_id, id, sizeof(results->query_id) - 1);
        results->query_id[sizeof(results->query_id) - 1] = '\0';
    }

    /* nextUri (replace any previous value) */
    free(results->next_uri);
    results->next_uri = NULL;
    if (json_object_object_get_ex(root, "nextUri", &field) &&
        json_object_is_type(field, json_type_string)) {
        results->next_uri = (SQLCHAR *)strdup(json_object_get_string(field));
    }

    /* Error object */
    if (json_object_object_get_ex(root, "error", &field) &&
        json_object_is_type(field, json_type_object)) {
        results->has_error = true;
        results->state = TRINO_QUERY_STATE_FAILED;

        json_object *sub = NULL;
        if (json_object_object_get_ex(field, "message", &sub)) {
            free(results->error_message);
            results->error_message = strdup(json_object_get_string(sub));
        }
        if (json_object_object_get_ex(field, "errorName", &sub) ||
            json_object_object_get_ex(field, "name", &sub)) {
            free(results->error_name);
            results->error_name = strdup(json_object_get_string(sub));
        }
        if (json_object_object_get_ex(field, "errorType", &sub)) {
            free(results->error_type);
            results->error_type = strdup(json_object_get_string(sub));
        }
        json_object_put(root);
        return SQL_ERROR;
    }

    /* State (from stats.state) */
    if (json_object_object_get_ex(root, "stats", &field) &&
        json_object_is_type(field, json_type_object)) {
        json_object *sub = NULL;
        if (json_object_object_get_ex(field, "state", &sub)) {
            const char *st = json_object_get_string(sub);
            if (st && strcmp(st, "FINISHED") == 0)
                results->state = TRINO_QUERY_STATE_FINISHED;
            else if (st && strcmp(st, "FAILED") == 0)
                results->state = TRINO_QUERY_STATE_FAILED;
            else if (st && strcmp(st, "CANCELLED") == 0)
                results->state = TRINO_QUERY_STATE_CANCELLED;
            else
                results->state = TRINO_QUERY_STATE_RUNNING;
        }
        if (json_object_object_get_ex(field, "processedRows", &sub)) {
            results->rows_processed = (SQLULEN)json_object_get_int64(sub);
        }
        if (json_object_object_get_ex(field, "processedBytes", &sub)) {
            results->bytes_processed = (SQLULEN)json_object_get_int64(sub);
        }
        if (json_object_object_get_ex(field, "elapsedTimeMillis", &sub)) {
            results->elapsed_time = json_object_get_double(sub);
        }
    }

    /* Columns: only parse once (the first page that carries them). */
    if (!results->columns &&
        json_object_object_get_ex(root, "columns", &field)) {
        SQLULEN count = 0;
        results->columns = trino_parse_columns_jsonc(field, &count);
        results->column_count = count;
    }

    /* Data rows: append this page's rows. */
    if (json_object_object_get_ex(root, "data", &field)) {
        if (append_data_rows(results, field) != 0) {
            json_object_put(root);
            return SQL_ERROR;
        }
    }

    json_object_put(root);
    return SQL_SUCCESS;
}

/* Helper to extract base type from parameterized types like decimal(10,2) */
static const char *get_base_type(const char *trino_type)
{
    static char base_type[128];
    size_t len = strlen(trino_type);
    if (len >= sizeof(base_type)) len = sizeof(base_type) - 1;
    strncpy(base_type, trino_type, len);
    base_type[len] = '\0';
    
    /* Strip parameters like (10,2) from decimal(10,2) */
    char *paren = strchr(base_type, '(');
    if (paren) *paren = '\0';
    
    return base_type;
}

/* Helper to get ODBC type from Trino type string */
static int trino_type_to_odbc_type_from_json(const char *trino_type)
{
    if (!trino_type) return SQL_VARCHAR;

    const char *base = get_base_type(trino_type);

    /* Handle complex types by converting to VARCHAR */
    if (strstr(base, "array") || strstr(base, "map") ||
        strstr(base, "row") || strstr(base, "json")) {
        return SQL_VARCHAR;
    }

    /* Numeric types */
    if (strcmp(base, "tinyint") == 0) return SQL_TINYINT;
    if (strcmp(base, "smallint") == 0) return SQL_SMALLINT;
    if (strcmp(base, "integer") == 0 || strcmp(base, "int") == 0) return SQL_INTEGER;
    if (strcmp(base, "bigint") == 0) return SQL_BIGINT;
    if (strcmp(base, "real") == 0) return SQL_REAL;
    if (strcmp(base, "double") == 0 || strcmp(base, "double precision") == 0) return SQL_DOUBLE;
    if (strcmp(base, "decimal") == 0 || strcmp(base, "numeric") == 0) return SQL_DECIMAL;

    /* String types */
    if (strcmp(base, "varchar") == 0) {
        return SQL_VARCHAR;
    }
    if (strcmp(base, "char") == 0) {
        return SQL_CHAR;
    }
    if (strcmp(base, "varbinary") == 0) {
        return SQL_VARBINARY;
    }

    /* Boolean */
    if (strcmp(base, "boolean") == 0) return SQL_BIT;

    /* Date/time types */
    if (strcmp(base, "date") == 0) return SQL_TYPE_DATE;
    if (strcmp(base, "time") == 0) return SQL_TYPE_TIME;
    if (strcmp(base, "timestamp") == 0) return SQL_TYPE_TIMESTAMP;

    /* UUID and others */
    if (strcmp(base, "uuid") == 0) return SQL_GUID;

    /* Default fallback */
    return SQL_VARCHAR;
}

/* Map Trino type string to ODBC SQL type - public wrapper */
SQLSMALLINT trino_type_to_odbc_type(const char *trino_type)
{
    return trino_type_to_odbc_type_from_json(trino_type);
}

/* Get human-readable name for Trino type (ODBC type) */
const char *trino_type_name(SQLSMALLINT odbc_type)
{
    switch (odbc_type) {
        case SQL_VARCHAR: return "VARCHAR";
        case SQL_CHAR: return "CHAR";
        case SQL_INTEGER: return "INTEGER";
        case SQL_BIGINT: return "BIGINT";
        case SQL_SMALLINT: return "SMALLINT";
        case SQL_TINYINT: return "TINYINT";
        case SQL_FLOAT: return "FLOAT";
        case SQL_REAL: return "REAL";
        case SQL_DOUBLE: return "DOUBLE";
        case SQL_DECIMAL: return "DECIMAL";
        case SQL_NUMERIC: return "NUMERIC";
        case SQL_BIT: return "BOOLEAN";
        case SQL_TYPE_DATE: return "DATE";
        case SQL_TYPE_TIME: return "TIME";
        case SQL_TYPE_TIMESTAMP: return "TIMESTAMP";
        case SQL_GUID: return "UUID";
        case SQL_VARBINARY: return "VARBINARY";
        default: return "UNKNOWN";
    }
}
