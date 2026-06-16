/* Prepared statement parameter handling */
#include "trino_odbc/statement.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Render a bound parameter as a SQL literal suitable for inlining into a query.
 * Reads `value` according to the bound C type using the correct width, and
 * quotes/escapes string values. Returns 0 on success, -1 on error (including
 * insufficient output space). The caller handles SQL NULL separately. */
static int format_parameter_value(SQLSMALLINT C_type, SQLPOINTER value,
                                   char *out, size_t out_size)
{
    if (!value || !out || out_size < 8) return -1;

    switch (C_type) {
        case SQL_C_CHAR: {
            /* String - wrap in single quotes and double any embedded quote. */
            if (strlen((char *)value) * 2 + 3 >= out_size) return -1;
            size_t pos = 0;
            out[pos++] = '\'';
            for (size_t i = 0; ((char *)value)[i] && pos < out_size - 2; i++) {
                if (((char *)value)[i] == '\'') {
                    out[pos++] = '\'';
                    out[pos++] = '\'';
                } else {
                    out[pos++] = ((char *)value)[i];
                }
            }
            out[pos++] = '\'';
            out[pos] = '\0';
            break;
        }
        case SQL_C_STINYINT:
        case SQL_C_TINYINT:
            snprintf(out, out_size, "%d", (int)*(signed char *)value);
            break;
        case SQL_C_UTINYINT:
            snprintf(out, out_size, "%u", (unsigned)*(unsigned char *)value);
            break;
        case SQL_C_SSHORT:
        case SQL_C_SHORT:
            snprintf(out, out_size, "%d", (int)*(short *)value);
            break;
        case SQL_C_USHORT:
            snprintf(out, out_size, "%u", (unsigned)*(unsigned short *)value);
            break;
        case SQL_C_SLONG:
        case SQL_C_LONG:
            snprintf(out, out_size, "%ld", (long)*(SQLINTEGER *)value);
            break;
        case SQL_C_ULONG:
            snprintf(out, out_size, "%lu", (unsigned long)*(SQLUINTEGER *)value);
            break;
        case SQL_C_SBIGINT:
            snprintf(out, out_size, "%lld", (long long)*(SQLBIGINT *)value);
            break;
        case SQL_C_UBIGINT:
            snprintf(out, out_size, "%llu", (unsigned long long)*(SQLUBIGINT *)value);
            break;
        case SQL_C_FLOAT:
            snprintf(out, out_size, "%.9g", (double)*(float *)value);
            break;
        case SQL_C_DOUBLE:
            snprintf(out, out_size, "%.17g", *(double *)value);
            break;
        case SQL_C_BIT:
            strncpy(out, (*(unsigned char *)value == 0) ? "false" : "true", out_size - 1);
            out[out_size - 1] = '\0';
            break;
        default:
            /* Unknown C type: refuse rather than emit a garbage literal. */
            return -1;
    }
    return 0;
}

/* ========================================================================
 * SQLBindParameter
 * ======================================================================== */

SQLRETURN SQLBindParameter(SQLHSTMT statement_handle, SQLUSMALLINT parameter_number,
                           SQLSMALLINT parameter_type, SQLSMALLINT C_type,
                           SQLSMALLINT SQL_type, SQLULEN column_size,
                           SQLSMALLINT decimal_digits, SQLPOINTER parameter_value_ptr,
                           SQLLEN buffer_length, SQLLEN *str_len_or_ind)
{
    (void)SQL_type;
    (void)column_size;
    (void)decimal_digits;
    (void)buffer_length;

    if (!statement_handle) return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt)) return SQL_INVALID_HANDLE;

    return trino_stmt_bind_param(stmt, parameter_number, parameter_type, C_type,
                                 column_size, decimal_digits,
                                 parameter_value_ptr, str_len_or_ind);
}

SQLRETURN trino_stmt_bind_param(trino_stmt_t *stmt, SQLUSMALLINT param_number,
                                SQLSMALLINT parameter_type, SQLSMALLINT C_type,
                                SQLULEN column_size, SQLSMALLINT decimal_digits,
                                SQLPOINTER parameter_value, SQLLEN *str_len_or_ind)
{
    (void)parameter_type;

    if (!stmt->ipd) {
        stmt->ipd = trino_desc_create();
        if (!stmt->ipd) return SQL_ERROR;
    }

    if (param_number == 0 || param_number > stmt->ipd->alloc_count) {
        return SQL_ERROR;
    }

    trino_desc_record_t *rec = &stmt->ipd->records[param_number - 1];
    rec->c_type = C_type;
    rec->column_size = column_size;
    rec->decimal_digits = decimal_digits;
    rec->data_ptr = parameter_value;
    rec->str_len_or_ind = str_len_or_ind;
    stmt->param_count = param_number > stmt->param_count ? param_number : stmt->param_count;

    return SQL_SUCCESS;
}

SQLRETURN SQLNumParams(SQLHSTMT statement_handle, SQLSMALLINT *parameter_count_ptr)
{
    if (!statement_handle || !parameter_count_ptr) return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt)) return SQL_INVALID_HANDLE;

    /* Count ? placeholders in SQL text */
    if (!stmt->sql_text) {
        *parameter_count_ptr = 0;
        return SQL_SUCCESS;
    }

    int count = 0;
    const char *p = stmt->sql_text;
    bool in_string = false;
    char string_char = '\0';

    while (*p) {
        if (in_string) {
            if (*p == string_char) {
                /* Check for escaped quote */
                if (*(p + 1) == string_char) {
                    p++; /* skip escaped quote */
                } else {
                    in_string = false;
                }
            }
        } else {
            if (*p == '\'' || *p == '"') {
                in_string = true;
                string_char = *p;
            } else if (*p == '?') {
                count++;
            }
        }
        p++;
    }

    *parameter_count_ptr = (SQLSMALLINT)count;
    return SQL_SUCCESS;
}

/* ========================================================================
 * Parameter substitution helper - replaces ? with actual values
 * ======================================================================== */

/* Append `len` bytes from `src` to a heap buffer, growing it as needed.
 * Updates *buf, *cap and *used. Returns 0 on success, -1 on allocation failure. */
static int append_bytes(char **buf, size_t *cap, size_t *used,
                        const char *src, size_t len)
{
    if (*used + len + 1 > *cap) {
        size_t newcap = (*cap ? *cap : 256);
        while (*used + len + 1 > newcap) newcap *= 2;
        char *grown = realloc(*buf, newcap);
        if (!grown) return -1;
        *buf = grown;
        *cap = newcap;
    }
    memcpy(*buf + *used, src, len);
    *used += len;
    (*buf)[*used] = '\0';
    return 0;
}

/* Replace each '?' placeholder in `sql` (outside of string literals) with the
 * corresponding bound parameter rendered as a SQL literal. Placeholders are
 * matched positionally. A parameter bound with str_len_or_ind == SQL_NULL_DATA
 * or with no data pointer is rendered as NULL. Returns a newly allocated string
 * (caller frees), or NULL on error. */
static char *substitute_parameters(trino_stmt_t *stmt, const char *sql,
                                   int param_count)
{
    if (!sql) return NULL;
    if (param_count == 0 || !stmt->ipd) {
        return strdup(sql);
    }

    size_t cap = strlen(sql) + 256;
    size_t used = 0;
    char *result = malloc(cap);
    if (!result) return NULL;
    result[0] = '\0';

    const char *src = sql;
    int param_index = 0;       /* 0-based index of the next placeholder */
    bool in_string = false;
    char string_char = '\0';

    while (*src) {
        if (in_string) {
            if (append_bytes(&result, &cap, &used, src, 1) != 0) goto fail;
            if (*src == string_char) {
                if (*(src + 1) == string_char) {
                    /* Escaped quote: copy the second quote too. */
                    src++;
                    if (append_bytes(&result, &cap, &used, src, 1) != 0) goto fail;
                } else {
                    in_string = false;
                }
            }
            src++;
            continue;
        }

        if (*src == '\'' || *src == '"') {
            in_string = true;
            string_char = *src;
            if (append_bytes(&result, &cap, &used, src, 1) != 0) goto fail;
            src++;
            continue;
        }

        if (*src == '?') {
            char buffer[1024];
            const char *literal = "NULL";
            size_t litlen = 4;

            if (param_index < (int)stmt->ipd->alloc_count) {
                trino_desc_record_t *rec = &stmt->ipd->records[param_index];
                bool is_null = (rec->str_len_or_ind &&
                                *rec->str_len_or_ind == SQL_NULL_DATA);
                if (rec->data_ptr && !is_null &&
                    format_parameter_value(rec->c_type, rec->data_ptr,
                                           buffer, sizeof(buffer)) == 0) {
                    literal = buffer;
                    litlen = strlen(buffer);
                }
            }

            if (append_bytes(&result, &cap, &used, literal, litlen) != 0) goto fail;
            param_index++;
            src++;
            continue;
        }

        if (append_bytes(&result, &cap, &used, src, 1) != 0) goto fail;
        src++;
    }

    return result;

fail:
    free(result);
    return NULL;
}

/* Count '?' placeholders in `sql` that lie outside string literals. */
static int count_placeholders(const char *sql)
{
    if (!sql) return 0;
    int count = 0;
    bool in_string = false;
    char string_char = '\0';
    for (const char *p = sql; *p; p++) {
        if (in_string) {
            if (*p == string_char) {
                if (*(p + 1) == string_char) p++;
                else in_string = false;
            }
        } else if (*p == '\'' || *p == '"') {
            in_string = true;
            string_char = *p;
        } else if (*p == '?') {
            count++;
        }
    }
    return count;
}

/* Produce the final SQL text to send to Trino, substituting any bound
 * parameters. If there are no placeholders, returns a copy of the input.
 * Caller frees the returned string. Returns NULL on allocation failure. */
char *trino_stmt_apply_params(trino_stmt_t *stmt, const SQLCHAR *sql)
{
    if (!stmt || !sql) return NULL;
    int n = count_placeholders((const char *)sql);
    return substitute_parameters(stmt, (const char *)sql, n);
}

/* ========================================================================
 * SQLParamData / SQLPutData (streaming parameter data)
 * ======================================================================== */

SQLRETURN SQLParamData(SQLHSTMT statement_handle, SQLPOINTER *value_ptr_ptr)
{
    (void)statement_handle;
    (void)value_ptr_ptr;
    /* Not implemented — Trino doesn't support streaming parameters */
    return SQL_NO_DATA;
}

SQLRETURN SQLPutData(SQLHSTMT statement_handle, SQLPOINTER data,
                     SQLLEN length)
{
    (void)statement_handle;
    (void)data;
    (void)length;
    /* Not implemented */
    return SQL_ERROR;
}
