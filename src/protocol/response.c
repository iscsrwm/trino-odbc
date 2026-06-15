/* Response parsing utilities for Trino protocol */

#include "trino_odbc/protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Parse column metadata from a JSON columns array string.
 * This is a simplified parser — a full implementation would use cJSON. */

static int count_json_array_elements(const char *json_array)
{
    if (!json_array || json_array[0] != '[') return 0;

    int count = 0;
    const char *p = json_array + 1; /* skip [ */

    while (*p) {
        /* Skip whitespace */
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        if (*p == ']') break;
        if (*p == '{') {
            count++;
            /* Find matching } */
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
        } else if (*p == ',') {
            p++;
        } else {
            p++;
        }
    }

    return count;
}

/* Extract a string field from a JSON object at a given index */
static char *extract_column_field(const char *columns_json, int col_idx, const char *field)
{
    /* Find the col_idx-th { object */
    const char *p = columns_json;
    int found = 0;

    while (*p && found <= col_idx) {
        if (*p == '{') {
            if (found == col_idx) {
                /* Search for "field" within this object */
                char search[256];
                snprintf(search, sizeof(search), "\"%s\"", field);
                const char *fpos = strstr(p, search);
                if (fpos) {
                    fpos = strchr(fpos + strlen(search), ':');
                    if (!fpos) return NULL;
                    fpos++;
                    while (*fpos && (*fpos == ' ' || *fpos == '\t')) fpos++;

                    if (*fpos == '"') {
                        fpos++;
                        const char *start = fpos;
                        const char *end = strchr(fpos, '"');
                        if (!end) return NULL;
                        size_t len = (size_t)(end - start);
                        char *result = malloc(len + 1);
                        if (result) {
                            memcpy(result, start, len);
                            result[len] = '\0';
                        }
                        return result;
                    }
                }
                /* Find the end of this object */
                int depth = 1;
                p++;
                while (*p && depth > 0) {
                    if (*p == '{') depth++;
                    else if (*p == '}') depth--;
                    p++;
                }
                break;
            }
            found++;
            /* Skip to next object */
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
        } else {
            p++;
        }
    }

    return NULL;
}

/* Parse columns from JSON response */
trino_column_meta_t *trino_parse_columns(const char *json, SQLULEN *column_count)
{
    if (!json || !column_count) return NULL;

    const char *cols_pos = strstr(json, "\"columns\"");
    if (!cols_pos) {
        *column_count = 0;
        return NULL;
    }

    const char *array_start = strchr(cols_pos, '[');
    if (!array_start) {
        *column_count = 0;
        return NULL;
    }

    int count = count_json_array_elements(array_start);
    if (count == 0) {
        *column_count = 0;
        return NULL;
    }

    trino_column_meta_t *columns = calloc((size_t)count, sizeof(trino_column_meta_t));
    if (!columns) {
        *column_count = 0;
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        char *name = extract_column_field(array_start, i, "name");
        char *type = extract_column_field(array_start, i, "type");

        if (name) {
            strncpy((char *)columns[i].name, name, SQL_MAX_IDENTIFIER_LEN);
            free(name);
        }

        if (type) {
            strncpy((char *)columns[i].type, type, TRINO_MAX_TYPE_NAME - 1);
            columns[i].odbc_type = trino_type_to_odbc_type(type);
            free(type);
        } else {
            strncpy((char *)columns[i].type, "varchar", TRINO_MAX_TYPE_NAME - 1);
            columns[i].odbc_type = SQL_VARCHAR;
        }

        columns[i].nullable = 1; /* SQL_NULLABLE — Trino defaults to nullable */
    }

    *column_count = (SQLULEN)count;
    return columns;
}
