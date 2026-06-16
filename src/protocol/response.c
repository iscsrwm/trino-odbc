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
            strncpy((char *)columns[i].name, name_str, SQL_MAX_IDENTIFIER_LEN);
            columns[i].name[SQL_MAX_IDENTIFIER_LEN] = '\0';
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
