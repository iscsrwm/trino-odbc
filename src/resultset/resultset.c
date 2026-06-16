#include "trino_odbc/resultset.h"
#include "trino_odbc/connection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ========================================================================
 * Result set lifecycle
 * ======================================================================== */

trino_resultset_t *trino_resultset_create(trino_query_results_t *qr)
{
    trino_resultset_t *rs = calloc(1, sizeof(*rs));
    if (!rs) return NULL;

    rs->query_results = qr;
    rs->current_row = 0;
    rs->row_count = qr ? qr->row_count : 0;
    rs->at_end = (rs->row_count == 0 && !qr);
    rs->needs_fetch = false;
    rs->cursor_type = SQL_CURSOR_FORWARD_ONLY;   /* Default to forward-only */

    return rs;
}

void trino_resultset_destroy(trino_resultset_t *rs)
{
    if (!rs) return;

    /* The result set takes ownership of the query results once created via
     * trino_resultset_create(), so free them here. */
    if (rs->query_results) {
        trino_query_results_free(rs->query_results);
        rs->query_results = NULL;
    }

    free(rs);
}

bool trino_resultset_at_end(const trino_resultset_t *rs)
{
    if (!rs) return true;
    return rs->at_end;
}

SQLLEN trino_resultset_row_count(const trino_resultset_t *rs)
{
    if (!rs) return 0;
    return (SQLLEN)rs->row_count;
}

/* ========================================================================
 * SQLFetch
 * ======================================================================== */

SQLRETURN SQLFetch(SQLHSTMT statement_handle)
{
    if (!statement_handle) return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt)) return SQL_INVALID_HANDLE;
    if (!stmt->resultset) return SQL_NO_DATA;

    trino_resultset_t *rs = (trino_resultset_t *)stmt->resultset;
    trino_query_results_t *qr = rs->query_results;
    if (!qr) return SQL_NO_DATA;

    /* current_row is a 1-based cursor position (0 == before the first row).
     * Row data for the current position lives at qr->rows[current_row - 1]. */

    /* If we have consumed all locally buffered rows, try to pull the next page
     * from the server before declaring end-of-data. */
    while (rs->current_row >= qr->row_count) {
        if (!qr->next_uri) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }

        trino_http_client_t *client =
            stmt->conn ? trino_conn_get_http_client(stmt->conn) : NULL;
        if (!client) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }

        SQLRETURN fr = trino_http_client_fetch_next(client, qr);
        trino_http_client_destroy(client);

        if (fr == SQL_ERROR) {
            return SQL_ERROR;
        }
        if (fr == SQL_NO_DATA && qr->next_uri == NULL &&
            rs->current_row >= qr->row_count) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }
        /* Loop again: a page may have arrived with zero new rows but a further
         * nextUri, or with rows that satisfy the cursor advance below. */
        if (qr->next_uri == NULL && rs->current_row >= qr->row_count) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }
    }

    /* Advance to the next row. */
    rs->current_row++;
    rs->row_count = qr->row_count;
    stmt->current_row = rs->current_row;

    return SQL_SUCCESS;
}

/* Advance the cursor by one row, pulling further pages as needed. Uses the
 * 1-based position model (see SQLFetch). `client` may be NULL, in which case
 * no further pages are fetched. */
SQLRETURN trino_resultset_fetch(trino_resultset_t *rs, trino_http_client_t *client)
{
    if (!rs || !rs->query_results) return SQL_ERROR;
    trino_query_results_t *qr = rs->query_results;

    while (rs->current_row >= qr->row_count) {
        if (!qr->next_uri || !client) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }
        SQLRETURN ret = trino_http_client_fetch_next(client, qr);
        if (ret == SQL_ERROR) return SQL_ERROR;
        if (qr->next_uri == NULL && rs->current_row >= qr->row_count) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }
    }

    rs->current_row++;
    rs->row_count = qr->row_count;
    return SQL_SUCCESS;
}

SQLRETURN trino_resultset_fetch_scroll(trino_resultset_t *rs, SQLINTEGER orientation,
                                       SQLROWID offset, trino_http_client_t *client)
{
    if (!rs || !rs->query_results) return SQL_ERROR;
    trino_query_results_t *qr = rs->query_results;

    switch (orientation) {
        case SQL_FETCH_NEXT:
            return trino_resultset_fetch(rs, client);

        case SQL_FETCH_FIRST:
            rs->current_row = 0;
            return trino_resultset_fetch(rs, client);

        case SQL_FETCH_ABSOLUTE:
            /* Position directly at the 1-based offset (only valid within the
             * rows already buffered locally). */
            if (offset > 0 && (SQLULEN)offset <= qr->row_count) {
                rs->current_row = (SQLULEN)offset;
                return SQL_SUCCESS;
            }
            return SQL_NO_DATA;

        case SQL_FETCH_RELATIVE: {
            if (rs->cursor_type == SQL_CURSOR_FORWARD_ONLY && offset < 1) {
                return SQL_ERROR;
            }
            SQLLEN new_pos = (SQLLEN)rs->current_row + offset;
            if (new_pos < 1 || (SQLULEN)new_pos > qr->row_count) {
                return SQL_NO_DATA;
            }
            rs->current_row = (SQLULEN)new_pos;
            return SQL_SUCCESS;
        }

        case SQL_FETCH_LAST:
            if (qr->row_count == 0) return SQL_NO_DATA;
            rs->current_row = qr->row_count;
            return SQL_SUCCESS;

        default:
            return SQL_ERROR;
    }
}

SQLRETURN SQLFetchScroll(SQLHSTMT statement_handle, SQLSMALLINT fetch_orientation,
                         SQLLEN fetch_offset)
{
    if (!statement_handle) return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt)) return SQL_INVALID_HANDLE;
    if (!stmt->resultset) return SQL_NO_DATA;

    trino_resultset_t *rs = (trino_resultset_t *)stmt->resultset;

    /* A client is only needed to page forward; create one on demand. */
    trino_http_client_t *client = NULL;
    if (fetch_orientation == SQL_FETCH_NEXT && stmt->conn) {
        client = trino_conn_get_http_client(stmt->conn);
    }

    SQLRETURN ret = trino_resultset_fetch_scroll(rs, fetch_orientation,
                                                  (SQLROWID)fetch_offset, client);

    if (client) trino_http_client_destroy(client);

    if (ret == SQL_SUCCESS) {
        stmt->current_row = rs->current_row;
    }

    return ret;
}

/* ========================================================================
 * SQLGetData
 * ======================================================================== */

SQLRETURN SQLGetData(SQLHSTMT statement_handle, SQLUSMALLINT column_number,
                     SQLSMALLINT target_type, SQLPOINTER target_value,
                     SQLLEN buffer_length, SQLLEN *str_len_or_ind)
{
    if (!statement_handle) return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt)) return SQL_INVALID_HANDLE;
    if (!stmt->resultset) return SQL_ERROR;

    trino_resultset_t *rs = (trino_resultset_t *)stmt->resultset;

    return trino_resultset_get_data(rs, column_number, target_type,
                                    target_value, (SQLBUFFER_LENGTH)buffer_length,
                                    str_len_or_ind);
}

SQLRETURN trino_resultset_get_data(trino_resultset_t *rs, SQLUSMALLINT col,
                                   SQLSMALLINT C_type, SQLPOINTER buffer,
                                   SQLBUFFER_LENGTH buffer_length, SQLLEN *str_len_or_ind)
{
    if (!rs || !rs->query_results) return SQL_ERROR;

    trino_query_results_t *qr = rs->query_results;

    /* current_row is 1-based; 0 means the cursor is positioned before the
     * first row (SQLFetch has not been called yet). */
    if (rs->current_row == 0 || rs->current_row > qr->row_count) {
        return SQL_NO_DATA;
    }
    SQLULEN row = rs->current_row - 1;

    if (col == 0 || col > qr->column_count) {
        return SQL_ERROR;
    }

    SQLULEN col_idx = col - 1;

    /* Get the raw string value from the row */
    const char *value = NULL;
    if (qr->rows && qr->rows[row] && qr->rows[row][col_idx]) {
        value = qr->rows[row][col_idx];
    }

    /* Check for NULL */
    if (str_len_or_ind) {
        if (!value) {
            *str_len_or_ind = SQL_NULL_DATA;
            return SQL_SUCCESS;
        }
    }

    if (!buffer || buffer_length == 0) {
        return SQL_SUCCESS;
    }

    /* Convert based on C type */
    switch (C_type) {
        case SQL_C_CHAR: {
            if (!value) {
                if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
                return SQL_SUCCESS;
            }
            size_t val_len = strlen(value);
            if (val_len >= (size_t)buffer_length) {
                if (str_len_or_ind) *str_len_or_ind = (SQLLEN)val_len;
                memcpy(buffer, value, (size_t)buffer_length - 1);
                ((char *)buffer)[buffer_length - 1] = '\0';
                return SQL_SUCCESS_WITH_INFO;
            }
            memcpy(buffer, value, val_len + 1);
            if (str_len_or_ind) *str_len_or_ind = (SQLLEN)val_len;
            return SQL_SUCCESS;
        }

        case SQL_C_LONG: {
            if (!value) {
                if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
                return SQL_SUCCESS;
            }
            *(SQLINTEGER *)buffer = (SQLINTEGER)atoi(value);
            if (str_len_or_ind) *str_len_or_ind = (SQLLEN)sizeof(SQLINTEGER);
            return SQL_SUCCESS;
        }

        case SQL_C_SBIGINT: {
            if (!value) {
                if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
                return SQL_SUCCESS;
            }
            *(SQLBIGINT *)buffer = (SQLBIGINT)atoll(value);
            if (str_len_or_ind) *str_len_or_ind = (SQLLEN)sizeof(SQLBIGINT);
            return SQL_SUCCESS;
        }

        case SQL_C_DOUBLE: {
            if (!value) {
                if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
                return SQL_SUCCESS;
            }
            *(SQLDOUBLE *)buffer = (SQLDOUBLE)atof(value);
            if (str_len_or_ind) *str_len_or_ind = (SQLLEN)sizeof(SQLDOUBLE);
            return SQL_SUCCESS;
        }

        case SQL_C_FLOAT: {
            if (!value) {
                if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
                return SQL_SUCCESS;
            }
            *(SQLFLOAT *)buffer = (SQLFLOAT)atof(value);
            if (str_len_or_ind) *str_len_or_ind = (SQLLEN)sizeof(SQLFLOAT);
            return SQL_SUCCESS;
        }

        case SQL_C_SHORT: {
            if (!value) {
                if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
                return SQL_SUCCESS;
            }
            *(SQLSHORT *)buffer = (SQLSHORT)atoi(value);
            if (str_len_or_ind) *str_len_or_ind = (SQLLEN)sizeof(SQLSHORT);
            return SQL_SUCCESS;
        }

        /* Note: SQL_C_BINARY has same value as SQL_C_SHORT (-2),
         * so it's handled by the default case below as string data */

        default:
            /* Default to string conversion */
            if (!value) {
                if (str_len_or_ind) *str_len_or_ind = SQL_NULL_DATA;
                return SQL_SUCCESS;
            }
            size_t val_len = strlen(value);
            if (val_len >= (size_t)buffer_length) {
                if (str_len_or_ind) *str_len_or_ind = (SQLLEN)val_len;
                memcpy(buffer, value, (size_t)buffer_length - 1);
                ((char *)buffer)[buffer_length - 1] = '\0';
                return SQL_SUCCESS_WITH_INFO;
            }
            memcpy(buffer, value, val_len + 1);
            if (str_len_or_ind) *str_len_or_ind = (SQLLEN)val_len;
            return SQL_SUCCESS;
    }
}

/* ========================================================================
 * SQLBindCol
 * ======================================================================== */

SQLRETURN SQLBindCol(SQLHSTMT statement_handle, SQLUSMALLINT column_number,
                     SQLSMALLINT target_type, SQLPOINTER target_value,
                     SQLLEN buffer_length, SQLLEN *str_len_or_ind)
{
    if (!statement_handle) return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt)) return SQL_INVALID_HANDLE;
    if (!stmt->ard) return SQL_ERROR;

    if (column_number == 0 || column_number > stmt->ard->record_count) {
        return SQL_ERROR;
    }

    trino_desc_record_t *rec = &stmt->ard->records[column_number - 1];
    rec->c_type = target_type;
    rec->data_ptr = target_value;
    rec->buffer_length = buffer_length;
    rec->str_len_or_ind = str_len_or_ind;

    return SQL_SUCCESS;
}
