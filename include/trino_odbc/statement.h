#ifndef TRINO_ODBC_STATEMENT_H
#define TRINO_ODBC_STATEMENT_H

#include "trino_odbc/core.h"
#include "trino_odbc/protocol.h"

/* Statement lifecycle */
trino_stmt_t *trino_stmt_create(trino_conn_t *conn);
void          trino_stmt_destroy(trino_stmt_t *stmt);

/* SQL execution */
SQLRETURN trino_stmt_prepare(trino_stmt_t *stmt, const SQLCHAR *sql, SQLINTEGER length);
SQLRETURN trino_stmt_execute(trino_stmt_t *stmt);
SQLRETURN trino_stmt_exec_direct(trino_stmt_t *stmt, const SQLCHAR *sql, SQLINTEGER length);
SQLRETURN trino_stmt_cancel(trino_stmt_t *stmt);
SQLRETURN trino_stmt_more_results(trino_stmt_t *stmt);

/* Parameter handling */
SQLRETURN trino_stmt_bind_param(trino_stmt_t *stmt, SQLUSMALLINT param_number,
                                SQLSMALLINT parameter_type, SQLSMALLINT C_type,
                                SQLULEN column_size, SQLSMALLINT decimal_digits,
                                SQLPOINTER parameter_value, SQLLEN *str_len_or_ind);
SQLULEN   trino_stmt_num_params(trino_stmt_t *stmt);

/* Produce final SQL with bound parameters substituted (caller frees result). */
char     *trino_stmt_apply_params(trino_stmt_t *stmt, const SQLCHAR *sql);

/* Returns true if the statement is a write/DDL operation (so an update row
 * count is reported instead of a result set). Handles case, leading comments
 * and CTEs. */
bool      trino_sql_is_write_op(const SQLCHAR *sql);

/* Statement attributes */
SQLRETURN trino_stmt_set_attr(trino_stmt_t *stmt, SQLINTEGER attr, SQLPOINTER value, SQLINTEGER str_len);
SQLRETURN trino_stmt_get_attr(trino_stmt_t *stmt, SQLINTEGER attr, SQLPOINTER value,
                              SQLINTEGER buffer_length, SQLINTEGER *str_len);

/* Result metadata */
SQLULEN trino_stmt_num_result_cols(trino_stmt_t *stmt);
SQLRETURN trino_stmt_col_attribute(trino_stmt_t *stmt, SQLUSMALLINT col,
                                   SQLINTEGER field, SQLCHAR *char_attr,
                                   SQLBUFFER_LENGTH buffer_length, SQLINTEGER *str_len,
                                   SQLLEN *numeric_attr);

#endif /* TRINO_ODBC_STATEMENT_H */
