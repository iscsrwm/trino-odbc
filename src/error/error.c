#include "trino_odbc/error.h"
#include <string.h>
#include <stdio.h>

void trino_diag_init(trino_diagnostics_t *diag)
{
    memset(diag, 0, sizeof(*diag));
    memcpy(diag->sqlstate, TRINO_SQLSTATE_SUCCESS, 5);
    diag->sqlstate[5] = '\0';
    diag->native_error = 0;
    diag->record_count = 0;
    diag->rec_number = 0;
}

void trino_diag_clear(trino_diagnostics_t *diag)
{
    diag->record_count = 0;
    diag->rec_number = 0;
    memcpy(diag->sqlstate, TRINO_SQLSTATE_SUCCESS, 5);
    diag->sqlstate[5] = '\0';
    diag->native_error = 0;
    memset(diag->records, 0, sizeof(diag->records));
}

void trino_diag_add(trino_diagnostics_t *diag, const char *sqlstate,
                    SQLINTEGER native_error, const char *message)
{
    if (diag->record_count >= MAX_DIAG_RECORDS) {
        return;
    }

    trino_diag_record_t *rec = &diag->records[diag->record_count];

    /* Copy sqlstate (always 5 chars) */
    if (sqlstate && strlen(sqlstate) >= 5) {
        memcpy(rec->sqlstate, sqlstate, 5);
    } else {
        memcpy(rec->sqlstate, TRINO_SQLSTATE_INVALID_SQLSTATE, 5);
    }
    rec->sqlstate[5] = '\0';

    rec->native_error = native_error;

    /* Copy message */
    if (message) {
        strncpy((char *)rec->message_text, message, TRINO_MAX_MESSAGE_LEN - 1);
        rec->message_text[TRINO_MAX_MESSAGE_LEN - 1] = '\0';
        rec->message_len = (SQLSMALLINT)strlen((char *)rec->message_text);
    } else {
        rec->message_text[0] = '\0';
        rec->message_len = 0;
    }

    diag->record_count++;

    /* First record sets the primary error */
    if (diag->record_count == 1) {
        memcpy(diag->sqlstate, rec->sqlstate, 5);
        diag->sqlstate[5] = '\0';
        diag->native_error = rec->native_error;
    }
}

void trino_diag_set_error(trino_diagnostics_t *diag, const char *sqlstate,
                          SQLINTEGER native_error, const char *message)
{
    trino_diag_clear(diag);
    trino_diag_add(diag, sqlstate, native_error, message);
}

/* Map Trino error types to SQLState */
static const char *map_trino_error_type(const char *error_type)
{
    if (!error_type)
        return TRINO_SQLSTATE_QUERY_FAILED;

    if (strstr(error_type, "INTERNAL_ERROR"))
        return TRINO_SQLSTATE_QUERY_FAILED;
    if (strstr(error_type, "USER_ERROR")) {
        /* Could be more specific but QUERY_FAILED is safe */
        return TRINO_SQLSTATE_QUERY_FAILED;
    }
    if (strstr(error_type, "COMMAND_ERROR"))
        return TRINO_SQLSTATE_QUERY_FAILED;
    if (strstr(error_type, "SPARK_ARGUMENT_ERROR"))
        return TRINO_SQLSTATE_SYNTAX_ERROR;

    return TRINO_SQLSTATE_QUERY_FAILED;
}

void trino_diag_from_trino_error(trino_diagnostics_t *diag, const char *error_name,
                                 const char *error_message, const char *error_type)
{
    const char *sqlstate = map_trino_error_type(error_type);
    SQLINTEGER native_error = 0;

    /* Build combined message */
    char combined[TRINO_MAX_MESSAGE_LEN];
    if (error_name && error_message) {
        snprintf(combined, sizeof(combined), "%s: %s", error_name, error_message);
    } else if (error_message) {
        snprintf(combined, sizeof(combined), "%s", error_message);
    } else if (error_name) {
        snprintf(combined, sizeof(combined), "%s", error_name);
    } else {
        snprintf(combined, sizeof(combined), "Unknown Trino error");
    }

    trino_diag_set_error(diag, sqlstate, native_error, combined);
}
