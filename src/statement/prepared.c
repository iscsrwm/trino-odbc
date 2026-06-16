/* Prepared statement parameter handling */
#include "trino_odbc/statement.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int format_parameter_value(SQLSMALLINT C_type, SQLPOINTER value,
                                   char *out, size_t out_size)
{
    if (!value || !out) return -1;

    /* Check for NULL */
    if (C_type == SQL_C_CHAR && value == NULL) {
        strncpy(out, "NULL", out_size - 1);
        out[out_size - 1] = '\0';
        return 0;
    }

    switch (C_type) {
        case SQL_C_CHAR:
            /* String - escape single quotes */
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
        case SQL_C_STINYINT:
        case SQL_C_SSHORTINT:
        case SQL_C_SLONGINT:
        case SQL_C_INT:
        case SQL_C_BIGINT:
            snprintf(out, out_size, "%lld", (long long)*(SQLBIGINT *)value);
            break;
        case SQL_C_FLOAT:
            snprintf(out, out_size, "%g", *(float *)value);
            break;
        case SQL_C_DOUBLE:
            snprintf(out, out_size, "%g", *(double *)value);
            break;
        case SQL_C_BIT:
            strncpy(out, (*(SQLCHAR *)value == 0) ? "false" : "true", out_size - 1);
            out[out_size - 1] = '\0';
            break;
        default:
            snprintf(out, out_size, "%p", value);
            break;
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
    (void)C_type;
    (void)column_size;
    (void)decimal_digits;
    (void)str_len_or_ind;

    if (!stmt->ipd) {
        stmt->ipd = trino_desc_create();
    }

    if (param_number == 0 || param_number > stmt->ipd->record_count) {
        return SQL_ERROR;
    }

    trino_desc_record_t *rec = &stmt->ipd->records[param_number - 1];
    rec->data_ptr = parameter_value;
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

static char *substitute_parameters(trino_stmt_t *stmt, const char *sql,
                                   int param_count)
{
    if (!sql || param_count == 0) {
        return sql ? strdup(sql) : NULL;
    }

    size_t result_size = strlen(sql) + 2048;
    char *result = malloc(result_size);
    if (!result) return NULL;

    const char *src = sql;
    char *dst = result;
    int param_num = 1;

    while (*src && (size_t)(dst - result) < result_size - 64) {
        if (*src == '?') {
            char buffer[512];
            bool found = false;

            for (int i = 0; i < stmt->ipd->record_count && !found; i++) {
                trino_desc_record_t *rec = &stmt->ipd->records[i];
                if (i + 1 == param_num && rec->data_ptr) {
                    int ret = format_parameter_value(SQL_C_CHAR, rec->data_ptr,
                                                     buffer, sizeof(buffer));
                    if (ret == 0) {
                        size_t len = strlen(buffer);
                        if ((size_t)(dst - result) + len < result_size - 64) {
                            memcpy(dst, buffer, len);
                            dst += len;
                        }
                        found = true;
                    }
                    param_num++;
                }
            }

            if (!found) {
                const char *null_str = "NULL";
                size_t len = strlen(null_str);
                if ((size_t)(dst - result) + len < result_size - 64) {
                    memcpy(dst, null_str, len);
                    dst += len;
                }
                param_num++;
            }
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    return result;
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

SQLRETURN SQLPutData(SQLHSTMT statement_handle, const SQLCHAR *data,
                     SQLULONG length)
{
    (void)statement_handle;
    (void)data;
    (void)length;
    /* Not implemented */
    return SQL_ERROR;
}
