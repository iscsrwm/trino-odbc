/* SQLSetPos - Row position manipulation (Trino read-only, returns SQL_ERROR) */

#include "trino_odbc.h"
#include "trino_odbc/error.h"
#include "trino_odbc/statement.h"

/* ========================================================================
 * SQLSetPos - Set cursor position and perform row operations
 * Trino is read-only, so this always returns SQL_ERROR
 * ======================================================================== */

SQLRETURN SQLSetPos(SQLHSTMT statement_handle, SQLSETPOSIROW row_number,
                    SQLUSMALLINT operation, SQLUSMALLINT lock_type)
{
    (void)row_number;
    (void)operation;
    (void)lock_type;

    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;

    /* Trino is read-only - no row updates allowed */
    trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_SQLSTATE, 0,
                         "Trino does not support row updates via SQLSetPos");
    return SQL_ERROR;
}
