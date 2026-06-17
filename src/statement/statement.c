#include "trino_odbc/statement.h"
#include "trino_odbc/connection.h"
#include "trino_odbc/resultset.h"
#include "trino_odbc/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ========================================================================
 * SQL classification helpers
 * ======================================================================== */

/* Advance past whitespace and SQL comments (-- line comments and / * * / block
 * comments) starting at *p. */
static const char *skip_ws_and_comments(const char *p)
{
    for (;;) {
        while (*p && isspace((unsigned char)*p))
            p++;

        if (p[0] == '-' && p[1] == '-') {
            p += 2;
            while (*p && *p != '\n')
                p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/'))
                p++;
            if (*p)
                p += 2; /* skip closing */
            continue;
        }
        break;
    }
    return p;
}

/* Compare the keyword starting at p (case-insensitive). Matches only if the
 * keyword is followed by a non-identifier character (so "INSERTED" does not
 * match "INSERT"). Returns the length matched, or 0. */
static size_t match_keyword(const char *p, const char *kw)
{
    size_t i = 0;
    while (kw[i]) {
        if (toupper((unsigned char)p[i]) != (unsigned char)kw[i])
            return 0;
        i++;
    }
    /* Ensure a word boundary follows the keyword. */
    unsigned char next = (unsigned char)p[i];
    if (next == '_' || isalnum(next))
        return 0;
    return i;
}

/* Determine whether a SQL statement is a write/DDL operation (so the driver
 * reports an update row count rather than a result set). Handles leading
 * whitespace/comments, case-insensitivity, and WITH ... CTEs (where the leading
 * keyword does not determine the statement type). */
bool trino_sql_is_write_op(const SQLCHAR *sql)
{
    if (!sql)
        return false;
    const char *p = skip_ws_and_comments((const char *)sql);

    /* A leading CTE (WITH ...) precedes the actual statement; skip the CTE
     * definitions to find the operative keyword. */
    if (match_keyword(p, "WITH")) {
        /* Walk to the statement that follows the CTE list. The CTE list is a
         * comma-separated set of "name AS ( ... )". We scan forward, tracking
         * parenthesis depth, until we reach a top-level keyword that is not
         * part of a CTE definition. */
        p += 4;
        int depth = 0;
        while (*p) {
            if (*p == '(')
                depth++;
            else if (*p == ')') {
                if (depth > 0)
                    depth--;
            } else if (depth == 0) {
                const char *q = skip_ws_and_comments(p);
                if (q != p) {
                    p = q;
                    continue;
                }
                if (match_keyword(p, "INSERT") || match_keyword(p, "UPDATE") ||
                    match_keyword(p, "DELETE") || match_keyword(p, "MERGE")) {
                    return true;
                }
                if (match_keyword(p, "SELECT")) {
                    return false;
                }
            }
            p++;
        }
        return false;
    }

    static const char *write_keywords[] = {
        "INSERT", "UPDATE", "DELETE",  "MERGE", "CREATE",  "DROP",    "ALTER", "TRUNCATE",
        "GRANT",  "REVOKE", "COMMENT", "CALL",  "REFRESH", "ANALYZE", NULL};
    for (int i = 0; write_keywords[i]; i++) {
        if (match_keyword(p, write_keywords[i]))
            return true;
    }
    return false;
}

/* ========================================================================
 * Statement lifecycle
 * ======================================================================== */

trino_stmt_t *trino_stmt_create(trino_conn_t *conn)
{
    trino_stmt_t *stmt = calloc(1, sizeof(*stmt));
    if (!stmt)
        return NULL;

    stmt->type = TRINO_HANDLE_STMT;
    stmt->conn = conn;
    stmt->query_timeout = conn ? conn->query_timeout : 300;
    stmt->cursor_type = SQL_CURSOR_FORWARD_ONLY;
    stmt->concurrency = SQL_CONCUR_READ_ONLY;
    stmt->max_rows = 0; /* unlimited */

    trino_diag_init(&stmt->diagnostics);
    pthread_mutex_init(&stmt->mutex, NULL);

    /* Create internal descriptors */
    stmt->ird = trino_desc_create();
    stmt->ard = trino_desc_create();

    if (conn) {
        trino_conn_register_stmt(conn, stmt);
    }

    return stmt;
}

void trino_stmt_destroy(trino_stmt_t *stmt)
{
    if (!stmt)
        return;

    if (stmt->conn) {
        trino_conn_unregister_stmt(stmt->conn, stmt);
    }

    free(stmt->sql_text);
    free(stmt->query_id);
    free(stmt->query_state);

    if (stmt->resultset) {
        trino_resultset_destroy((trino_resultset_t *)stmt->resultset);
    }

    if (stmt->ipd)
        trino_desc_destroy(stmt->ipd);
    if (stmt->ird)
        trino_desc_destroy(stmt->ird);
    if (stmt->ard)
        trino_desc_destroy(stmt->ard);

    free(stmt->row_status);

    pthread_mutex_destroy(&stmt->mutex);
    free(stmt);
}

/* ========================================================================
 * SQLPrepare
 * ======================================================================== */

SQLRETURN SQLPrepare(SQLHSTMT statement_handle, SQLCHAR *statement_text,
                     SQLINTEGER text_length)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;

    pthread_mutex_lock(&stmt->mutex);

    /* Free previous SQL text */
    free(stmt->sql_text);
    stmt->prepared = false;
    stmt->executed = false;
    stmt->at_end = false;

    if (stmt->resultset) {
        trino_resultset_destroy((trino_resultset_t *)stmt->resultset);
        stmt->resultset = NULL;
    }

    /* Store SQL text */
    if (text_length == SQL_NTS) {
        stmt->sql_length = (SQLINTEGER)strlen((char *)statement_text);
    } else {
        stmt->sql_length = text_length;
    }

    stmt->sql_text = malloc((size_t)stmt->sql_length + 1);
    if (!stmt->sql_text) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_MEMORY_ALLOCATION, 0,
                             "Memory allocation failed");
        pthread_mutex_unlock(&stmt->mutex);
        return SQL_ERROR;
    }
    memcpy(stmt->sql_text, statement_text, (size_t)stmt->sql_length);
    stmt->sql_text[stmt->sql_length] = '\0';

    /* Trino doesn't have a separate "prepare" step — preparation is implicit.
     * We mark it as prepared for ODBC compliance. */
    stmt->prepared = true;

    pthread_mutex_unlock(&stmt->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLExecute
 * ======================================================================== */

SQLRETURN SQLExecute(SQLHSTMT statement_handle)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->prepared) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CURSOR, 0,
                             "Statement not prepared");
        return SQL_ERROR;
    }
    if (!stmt->sql_text) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CURSOR, 0,
                             "No SQL text");
        return SQL_ERROR;
    }

    return trino_stmt_exec_direct(stmt, stmt->sql_text, stmt->sql_length);
}

/* ========================================================================
 * SQLExecDirect
 * ======================================================================== */

SQLRETURN SQLExecDirect(SQLHSTMT statement_handle, SQLCHAR *statement_text,
                        SQLINTEGER text_length)
{
    trino_log("SQLExecDirect: ENTRY handle=%p", (void *)statement_handle);
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;

    /* Prepare first */
    SQLRETURN ret = SQLPrepare(statement_handle, statement_text, text_length);
    if (ret != SQL_SUCCESS)
        return ret;

    return trino_stmt_exec_direct(stmt, stmt->sql_text, stmt->sql_length);
}

SQLRETURN trino_stmt_exec_direct(trino_stmt_t *stmt, const SQLCHAR *sql,
                                 SQLINTEGER length)
{
    (void)length;

    if (!stmt || !sql || !stmt->conn) {
        if (stmt) {
            trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                                 "Invalid connection");
        }
        return SQL_ERROR;
    }

    pthread_mutex_lock(&stmt->mutex);

    /* Free previous result set */
    if (stmt->resultset) {
        trino_resultset_destroy((trino_resultset_t *)stmt->resultset);
        stmt->resultset = NULL;
    }

    /* Detect write/DDL operations so we report an update count, not a result
     * set. Handles case, leading comments, and CTEs. */
    bool is_write_op = trino_sql_is_write_op(sql);

    /* Get HTTP client */
    trino_http_client_t *client = trino_conn_get_http_client(stmt->conn);
    if (!client) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_REQUEST_FAILED, 0,
                             "Failed to create HTTP client");
        pthread_mutex_unlock(&stmt->mutex);
        return SQL_ERROR;
    }

    /* Set timeout */
    client->request_timeout = stmt->query_timeout;
    curl_easy_setopt(client->easy_handle, CURLOPT_TIMEOUT, (long)stmt->query_timeout);

    /* Substitute any bound parameters into the SQL text before sending. */
    char *final_sql = trino_stmt_apply_params(stmt, sql);
    if (!final_sql) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_MEMORY_ALLOCATION, 0,
                             "Failed to bind parameters");
        pthread_mutex_unlock(&stmt->mutex);
        return SQL_ERROR;
    }

    /* Execute query */
    SQLRETURN retcode = SQL_SUCCESS;
    trino_query_results_t *results =
        trino_http_client_query(client, (const SQLCHAR *)final_sql, &retcode);
    free(final_sql);

    if (!results) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_REQUEST_FAILED, 0,
                             "Query execution failed");
        pthread_mutex_unlock(&stmt->mutex);
        return SQL_ERROR;
    }

    if (results->has_error) {
        trino_diag_from_trino_error(&stmt->diagnostics, results->error_name,
                                    results->error_message, results->error_type);
        trino_query_results_free(results);
        pthread_mutex_unlock(&stmt->mutex);
        return SQL_ERROR;
    }

    /* Store query ID */
    free(stmt->query_id);
    stmt->query_id = strdup(results->query_id);

    /* Store query statistics */
    stmt->rows_processed = results->rows_processed;
    stmt->bytes_processed = results->bytes_processed;
    stmt->elapsed_time_ms = (SQLULEN)(results->elapsed_time * 1000.0);

    /* For write operations, set row_count from stats and don't create resultset */
    if (is_write_op) {
        stmt->row_count = (SQLLEN)results->rows_processed;
        stmt->column_count = 0;
        stmt->executed = true;
        stmt->at_end = true;
        /* Write operations don't return result sets; the stats we needed have
         * been copied above, so release the results now. */
        trino_query_results_free(results);
        stmt->resultset = NULL;
    } else {
        /* Read operation - set up result set */
        stmt->resultset = trino_resultset_create(results);
        if (stmt->resultset) {
            ((trino_resultset_t *)stmt->resultset)->cursor_type = stmt->cursor_type;
        }
        stmt->executed = true;
        stmt->at_end = false;
        stmt->current_row = 0;
        stmt->column_count = results->column_count;

        /* Update IRD with column metadata */
        if (results->columns && results->column_count > 0) {
            for (SQLULEN i = 0; i < results->column_count; i++) {
                trino_column_meta_t *col = &results->columns[i];
                trino_desc_record_t *rec = &stmt->ird->records[i];
                rec->sql_type = col->odbc_type;
                rec->nullable = col->nullable;
                /* column_name/type_name are TRINO_MAX_IDENTIFIER_LEN+1 bytes;
                 * copy at most LEN bytes and always NUL-terminate. */
                strncpy((char *)rec->column_name, (char *)col->name,
                        TRINO_MAX_IDENTIFIER_LEN);
                rec->column_name[TRINO_MAX_IDENTIFIER_LEN] = '\0';
                strncpy((char *)rec->type_name, (char *)col->type,
                        TRINO_MAX_IDENTIFIER_LEN);
                rec->type_name[TRINO_MAX_IDENTIFIER_LEN] = '\0';
            }
            stmt->ird->record_count = results->column_count;
        }
    }

    pthread_mutex_unlock(&stmt->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLCancel
 * ======================================================================== */

SQLRETURN SQLCancel(SQLHSTMT statement_handle)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->query_id) {
        return SQL_SUCCESS; /* nothing to cancel */
    }

    trino_http_client_t *client = trino_conn_get_http_client(stmt->conn);
    if (!client)
        return SQL_ERROR;

    SQLRETURN ret = trino_http_client_kill_query(client, stmt->query_id);

    if (ret == SQL_SUCCESS) {
        stmt->executed = false;
        stmt->at_end = true;
    }

    /* client is owned by the connection; do not destroy here. */
    return ret;
}

/* ========================================================================
 * SQLMoreResults
 * ======================================================================== */

SQLRETURN SQLMoreResults(SQLHSTMT statement_handle)
{
    /* Trino doesn't support multiple result sets from a single statement */
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    return SQL_NO_DATA;
}

/* ========================================================================
 * SQLNumResultCols
 * ======================================================================== */

SQLRETURN SQLNumResultCols(SQLHSTMT statement_handle, SQLSMALLINT *column_count_ptr)
{
    if (!statement_handle || !column_count_ptr)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;

    pthread_mutex_lock(&stmt->mutex);
    *column_count_ptr = (SQLSMALLINT)stmt->column_count;
    pthread_mutex_unlock(&stmt->mutex);

    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLRowCount
 * ======================================================================== */

SQLRETURN SQLRowCount(SQLHSTMT statement_handle, SQLLEN *row_count_ptr)
{
    if (!statement_handle || !row_count_ptr)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;

    pthread_mutex_lock(&stmt->mutex);
    *row_count_ptr = stmt->row_count;
    pthread_mutex_unlock(&stmt->mutex);

    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLColAttribute
 * ======================================================================== */

SQLRETURN SQLColAttribute(SQLHSTMT statement_handle, SQLUSMALLINT column_number,
                          SQLUSMALLINT field_identifier, SQLPOINTER character_attribute,
                          SQLSMALLINT buffer_length, SQLSMALLINT *string_length,
                          SQLLEN *numeric_attribute)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;

    SQLINTEGER str_len = 0;
    SQLRETURN ret = trino_stmt_col_attribute(
        stmt, column_number, (SQLINTEGER)field_identifier, (SQLCHAR *)character_attribute,
        (SQLBUFFER_LENGTH)buffer_length, &str_len, numeric_attribute);
    if (string_length)
        *string_length = (SQLSMALLINT)str_len;
    return ret;
}

SQLRETURN trino_stmt_col_attribute(trino_stmt_t *stmt, SQLUSMALLINT col, SQLINTEGER field,
                                   SQLCHAR *char_attr, SQLBUFFER_LENGTH buffer_length,
                                   SQLINTEGER *str_len, SQLLEN *numeric_attr)
{
    if (!stmt || !stmt->ird || col == 0 || col > stmt->ird->record_count) {
        return SQL_ERROR;
    }

    trino_desc_record_t *rec = &stmt->ird->records[col - 1];

    switch (field) {
        case SQL_DESC_LABEL:
        case SQL_DESC_NAME:
            if (char_attr && buffer_length > 0) {
                strncpy((char *)char_attr, (char *)rec->column_name,
                        (size_t)buffer_length - 1);
                ((char *)char_attr)[buffer_length - 1] = '\0';
                if (str_len)
                    *str_len = (SQLINTEGER)strlen((char *)char_attr);
            }
            break;

        case SQL_DESC_TYPE:
            if (numeric_attr)
                *numeric_attr = (SQLLEN)rec->sql_type;
            break;

        case SQL_DESC_TYPE_NAME:
            if (char_attr && buffer_length > 0) {
                strncpy((char *)char_attr, (char *)rec->type_name,
                        (size_t)buffer_length - 1);
                ((char *)char_attr)[buffer_length - 1] = '\0';
                if (str_len)
                    *str_len = (SQLINTEGER)strlen((char *)char_attr);
            }
            break;

        case SQL_DESC_PRECISION:
            if (numeric_attr)
                *numeric_attr = (SQLLEN)rec->column_size;
            break;

        case SQL_DESC_SCALE:
            if (numeric_attr)
                *numeric_attr = (SQLLEN)rec->decimal_digits;
            break;

        case SQL_DESC_NULLABLE:
            if (numeric_attr)
                *numeric_attr = (SQLLEN)rec->nullable;
            break;

        case SQL_DESC_DISPLAY_SIZE:
            if (numeric_attr)
                *numeric_attr = (SQLLEN)rec->column_size;
            break;

        default: break;
    }

    return SQL_SUCCESS;
}

/* ========================================================================
 * Statement attributes
 * ======================================================================== */

SQLRETURN SQL_API SQLSetStmtAttr(SQLHSTMT statement_handle, SQLINTEGER attribute,
                                  SQLPOINTER value_ptr, SQLINTEGER string_length)
{
    trino_log("SQLSetStmtAttr: ENTRY attr=%d stmt=%p value=%p strlen=%d", 
              (int)attribute, (void *)statement_handle, value_ptr, (int)string_length);
    
    // Simplified version - just log and return success for now
    trino_log("SQLSetStmtAttr: EXIT returning SQL_SUCCESS");
    return SQL_SUCCESS;
    
    /* Original code commented out for debugging
    if (!statement_handle) {
        trino_log("SQLSetStmtAttr: statement_handle is NULL");
        return SQL_INVALID_HANDLE;
    }

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    trino_log("SQLSetStmtAttr: checking validity, stmt->type=%d", stmt ? stmt->type : -1);
    if (!trino_stmt_valid(stmt)) {
        trino_log("SQLSetStmtAttr: invalid statement handle");
        return SQL_INVALID_HANDLE;
    }

    trino_log("SQLSetStmtAttr: calling trino_stmt_set_attr");
    SQLRETURN ret = trino_stmt_set_attr(stmt, attribute, value_ptr, string_length);
    trino_log("SQLSetStmtAttr: exit ret=%d", ret);
    return ret;
    */
}

SQLRETURN trino_stmt_set_attr(trino_stmt_t *stmt, SQLINTEGER attr, SQLPOINTER value,
                              SQLINTEGER str_len)
{
    (void)str_len;
    pthread_mutex_lock(&stmt->mutex);

    /* SQLSetStmtAttr passes these integer attributes by value in the ValuePtr
     * argument (cast to a pointer), not as a pointer to the value. Dereferencing
     * it crashes. Pointer-valued attributes (row-status arrays, bind offsets,
     * etc.) are not handled here. */
    SQLUINTEGER uval = (SQLUINTEGER)(SQLULEN)value;
    SQLULEN ulval = (SQLULEN)value;

    switch (attr) {
        case SQL_ATTR_QUERY_TIMEOUT: stmt->query_timeout = uval; break;

        case SQL_ATTR_CURSOR_TYPE: stmt->cursor_type = uval; break;

        case SQL_ATTR_CONCURRENCY: stmt->concurrency = uval; break;

        case SQL_ATTR_MAX_ROWS: stmt->max_rows = ulval; break;

        case SQL_ATTR_ROW_ARRAY_SIZE: stmt->row_array_size = ulval; break;

        default: break;
    }

    pthread_mutex_unlock(&stmt->mutex);
    return SQL_SUCCESS;
}

SQLRETURN SQLGetStmtAttr(SQLHSTMT statement_handle, SQLINTEGER attribute,
                         SQLPOINTER value_ptr, SQLINTEGER buffer_length,
                         SQLINTEGER *string_length_ptr)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;

    return trino_stmt_get_attr(stmt, attribute, value_ptr, buffer_length,
                               string_length_ptr);
}

SQLRETURN trino_stmt_get_attr(trino_stmt_t *stmt, SQLINTEGER attr, SQLPOINTER value,
                              SQLINTEGER buffer_length, SQLINTEGER *str_len)
{
    (void)buffer_length;

    pthread_mutex_lock(&stmt->mutex);

    switch (attr) {
        case SQL_ATTR_QUERY_TIMEOUT:
            *(SQLUINTEGER *)value = stmt->query_timeout;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CURSOR_TYPE:
            *(SQLUINTEGER *)value = stmt->cursor_type;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CONCURRENCY:
            *(SQLUINTEGER *)value = stmt->concurrency;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_MAX_ROWS:
            *(SQLULEN *)value = stmt->max_rows;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLULEN);
            break;

        case SQL_ATTR_ROW_ARRAY_SIZE:
            *(SQLULEN *)value = stmt->row_array_size;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLULEN);
            break;

        default: pthread_mutex_unlock(&stmt->mutex); return SQL_SUCCESS;
    }

    pthread_mutex_unlock(&stmt->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * Connection-level functions
 * ======================================================================== */

SQLRETURN SQLSetConnectAttr(SQLHDBC connection_handle, SQLINTEGER attribute,
                            SQLPOINTER value_ptr, SQLINTEGER string_length)
{
    trino_log("SQLSetConnectAttr: attr=%d value=%p", (int)attribute,
              (void *)value_ptr);
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    return trino_conn_set_attr(conn, attribute, value_ptr, string_length);
}

SQLRETURN SQLGetConnectAttr(SQLHDBC connection_handle, SQLINTEGER attribute,
                            SQLPOINTER value_ptr, SQLINTEGER buffer_length,
                            SQLINTEGER *string_length_ptr)
{
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    return trino_conn_get_attr(conn, attribute, value_ptr, buffer_length,
                               string_length_ptr);
}

/* Unicode bridges. The connection attributes used during Open() (login/connect
 * timeout, autocommit, access mode, txn isolation) are all numeric and so are
 * width-agnostic; delegate straight to the ANSI implementations. The DM, once
 * the driver is in Unicode mode, calls these W forms. */
SQLRETURN SQLSetConnectAttrW(SQLHDBC connection_handle, SQLINTEGER attribute,
                             SQLPOINTER value_ptr, SQLINTEGER string_length)
{
    return SQLSetConnectAttr(connection_handle, attribute, value_ptr, string_length);
}

SQLRETURN SQLGetConnectAttrW(SQLHDBC connection_handle, SQLINTEGER attribute,
                             SQLPOINTER value_ptr, SQLINTEGER buffer_length,
                             SQLINTEGER *string_length_ptr)
{
    return SQLGetConnectAttr(connection_handle, attribute, value_ptr, buffer_length,
                             string_length_ptr);
}
