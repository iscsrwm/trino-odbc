/* Prepared statement parameter handling */

#include "trino_odbc/statement.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
