/* SQLGetDiagRec / SQLGetDiagField: the entry points the driver manager and
 * applications use to retrieve diagnostic records (SQLSTATE, native error,
 * message) after a function returns SQL_ERROR or SQL_SUCCESS_WITH_INFO.
 *
 * Without these, applications (e.g. .NET's OdbcConnection) surface failures as
 * an OdbcException with an empty message and no error records.
 */

#include "trino_odbc/core.h"
#include "trino_odbc/error.h"
#include <string.h>

/* Resolve the diagnostics block for any handle. All handle structs begin with
 * a trino_handle_type_t `type` field, so we can dispatch on it. */
static trino_diagnostics_t *diag_for_handle(SQLSMALLINT handle_type, SQLHANDLE handle)
{
    if (!handle)
        return NULL;

    switch (handle_type) {
        case SQL_HANDLE_ENV: {
            trino_env_t *env = (trino_env_t *)handle;
            return trino_env_valid(env) ? &env->diagnostics : NULL;
        }
        case SQL_HANDLE_DBC: {
            trino_conn_t *conn = (trino_conn_t *)handle;
            return trino_conn_valid(conn) ? &conn->diagnostics : NULL;
        }
        case SQL_HANDLE_STMT: {
            trino_stmt_t *stmt = (trino_stmt_t *)handle;
            return trino_stmt_valid(stmt) ? &stmt->diagnostics : NULL;
        }
        default: return NULL;
    }
}

/* ========================================================================
 * SQLGetDiagRec — return SQLSTATE, native error, and message for a record
 * ======================================================================== */

SQLRETURN SQLGetDiagRec(SQLSMALLINT handle_type, SQLHANDLE handle, SQLSMALLINT rec_number,
                        SQLCHAR *sqlstate, SQLINTEGER *native_error,
                        SQLCHAR *message_text, SQLSMALLINT buffer_length,
                        SQLSMALLINT *text_length)
{
    if (rec_number < 1)
        return SQL_ERROR;

    trino_diagnostics_t *diag = diag_for_handle(handle_type, handle);
    if (!diag)
        return SQL_INVALID_HANDLE;

    if (rec_number > diag->record_count)
        return SQL_NO_DATA;

    trino_diag_record_t *rec = &diag->records[rec_number - 1];

    if (sqlstate) {
        memcpy(sqlstate, rec->sqlstate, 5);
        sqlstate[5] = '\0';
    }
    if (native_error)
        *native_error = rec->native_error;

    SQLSMALLINT msg_len = rec->message_len;
    if (message_text && buffer_length > 0) {
        SQLSMALLINT copy = msg_len;
        if (copy > buffer_length - 1)
            copy = buffer_length - 1;
        memcpy(message_text, rec->message_text, (size_t)copy);
        message_text[copy] = '\0';
        if (text_length)
            *text_length = msg_len;
        return (msg_len > copy) ? SQL_SUCCESS_WITH_INFO : SQL_SUCCESS;
    }
    if (text_length)
        *text_length = msg_len;
    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLGetDiagField — return a single field of a diagnostic record (or header)
 * ======================================================================== */

SQLRETURN SQLGetDiagField(SQLSMALLINT handle_type, SQLHANDLE handle,
                          SQLSMALLINT rec_number, SQLSMALLINT diag_identifier,
                          SQLPOINTER diag_info, SQLSMALLINT buffer_length,
                          SQLSMALLINT *string_length)
{
    trino_diagnostics_t *diag = diag_for_handle(handle_type, handle);
    if (!diag)
        return SQL_INVALID_HANDLE;

    switch (diag_identifier) {
        /* Header fields (rec_number is ignored). */
        case SQL_DIAG_NUMBER:
            if (diag_info)
                *(SQLINTEGER *)diag_info = diag->record_count;
            return SQL_SUCCESS;
        case SQL_DIAG_RETURNCODE:
            if (diag_info)
                *(SQLRETURN *)diag_info =
                    diag->record_count > 0 ? SQL_ERROR : SQL_SUCCESS;
            return SQL_SUCCESS;
        default: break;
    }

    if (rec_number < 1 || rec_number > diag->record_count)
        return SQL_NO_DATA;

    trino_diag_record_t *rec = &diag->records[rec_number - 1];

    switch (diag_identifier) {
        case SQL_DIAG_SQLSTATE:
            if (diag_info && buffer_length > 0) {
                memcpy(diag_info, rec->sqlstate, 5);
                ((char *)diag_info)[5 < buffer_length ? 5 : buffer_length - 1] = '\0';
            }
            if (string_length)
                *string_length = 5;
            return SQL_SUCCESS;
        case SQL_DIAG_NATIVE:
            if (diag_info)
                *(SQLINTEGER *)diag_info = rec->native_error;
            return SQL_SUCCESS;
        case SQL_DIAG_MESSAGE_TEXT:
            if (diag_info && buffer_length > 0) {
                SQLSMALLINT copy = rec->message_len;
                if (copy > buffer_length - 1)
                    copy = buffer_length - 1;
                memcpy(diag_info, rec->message_text, (size_t)copy);
                ((char *)diag_info)[copy] = '\0';
            }
            if (string_length)
                *string_length = rec->message_len;
            return SQL_SUCCESS;
        default: return SQL_SUCCESS;
    }
}
