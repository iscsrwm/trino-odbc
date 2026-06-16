#ifndef TRINO_ODBC_RESULTSET_H
#define TRINO_ODBC_RESULTSET_H

#include "trino_odbc/core.h"
#include "trino_odbc/protocol.h"

/* ========================================================================
 * Result set — wraps Trino query results for ODBC consumption
 * ======================================================================== */

typedef struct {
    trino_query_results_t *query_results;
    SQLULEN current_row;
    SQLULEN row_count;
    bool at_end;
    bool needs_fetch;
    SQLUINTEGER cursor_type; /* SQL_CURSOR_FORWARD_ONLY, etc. */
} trino_resultset_t;

/* Create/destroy result set */
trino_resultset_t *trino_resultset_create(trino_query_results_t *qr);
void trino_resultset_destroy(trino_resultset_t *rs);

/* Fetch next row (returns SQL_NO_DATA when done) */
SQLRETURN trino_resultset_fetch(trino_resultset_t *rs, trino_http_client_t *client);
SQLRETURN trino_resultset_fetch_scroll(trino_resultset_t *rs, SQLINTEGER orientation,
                                       SQLROWID offset, trino_http_client_t *client);

/* Get column data for current row */
SQLRETURN trino_resultset_get_data(trino_resultset_t *rs, SQLUSMALLINT col,
                                   SQLSMALLINT C_type, SQLPOINTER buffer,
                                   SQLBUFFER_LENGTH buffer_length,
                                   SQLLEN *str_len_or_ind);

/* Check if at end */
bool trino_resultset_at_end(const trino_resultset_t *rs);

/* Get row count */
SQLLEN trino_resultset_row_count(const trino_resultset_t *rs);

#endif /* TRINO_ODBC_RESULTSET_H */
