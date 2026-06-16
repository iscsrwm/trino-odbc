#include "trino_odbc/resultset.h"
#include "trino_odbc/connection.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

/* ========================================================================
 * Result set lifecycle
 * ======================================================================== */

trino_resultset_t *trino_resultset_create(trino_query_results_t *qr)
{
    trino_resultset_t *rs = calloc(1, sizeof(*rs));
    if (!rs)
        return NULL;

    rs->query_results = qr;
    rs->current_row = 0;
    rs->row_count = qr ? qr->row_count : 0;
    rs->at_end = (rs->row_count == 0 && !qr);
    rs->needs_fetch = false;
    rs->cursor_type = SQL_CURSOR_FORWARD_ONLY; /* Default to forward-only */

    return rs;
}

void trino_resultset_destroy(trino_resultset_t *rs)
{
    if (!rs)
        return;

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
    if (!rs)
        return true;
    return rs->at_end;
}

SQLLEN trino_resultset_row_count(const trino_resultset_t *rs)
{
    if (!rs)
        return 0;
    return (SQLLEN)rs->row_count;
}

/* ========================================================================
 * SQLFetch
 * ======================================================================== */

SQLRETURN SQLFetch(SQLHSTMT statement_handle)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->resultset)
        return SQL_NO_DATA;

    trino_resultset_t *rs = (trino_resultset_t *)stmt->resultset;
    trino_query_results_t *qr = rs->query_results;
    if (!qr)
        return SQL_NO_DATA;

    /* current_row is a 1-based cursor position (0 == before the first row).
     * Row data for the current position lives at qr->rows[current_row - 1]. */

    /* If we have consumed all locally buffered rows, try to pull the next page
     * from the server before declaring end-of-data. */
    while (rs->current_row >= qr->row_count) {
        if (!qr->next_uri) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }

        /* client is owned by the connection; reused across pages, not freed. */
        trino_http_client_t *client =
            stmt->conn ? trino_conn_get_http_client(stmt->conn) : NULL;
        if (!client) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }

        SQLRETURN fr = trino_http_client_fetch_next(client, qr);

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
    if (!rs || !rs->query_results)
        return SQL_ERROR;
    trino_query_results_t *qr = rs->query_results;

    while (rs->current_row >= qr->row_count) {
        if (!qr->next_uri || !client) {
            rs->at_end = true;
            return SQL_NO_DATA;
        }
        SQLRETURN ret = trino_http_client_fetch_next(client, qr);
        if (ret == SQL_ERROR)
            return SQL_ERROR;
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
    if (!rs || !rs->query_results)
        return SQL_ERROR;
    trino_query_results_t *qr = rs->query_results;

    switch (orientation) {
        case SQL_FETCH_NEXT: return trino_resultset_fetch(rs, client);

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
            if (qr->row_count == 0)
                return SQL_NO_DATA;
            rs->current_row = qr->row_count;
            return SQL_SUCCESS;

        default: return SQL_ERROR;
    }
}

SQLRETURN SQLFetchScroll(SQLHSTMT statement_handle, SQLSMALLINT fetch_orientation,
                         SQLLEN fetch_offset)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->resultset)
        return SQL_NO_DATA;

    trino_resultset_t *rs = (trino_resultset_t *)stmt->resultset;

    /* A client is only needed to page forward; it is owned by the connection
     * and reused, so it is not destroyed here. */
    trino_http_client_t *client = NULL;
    if (fetch_orientation == SQL_FETCH_NEXT && stmt->conn) {
        client = trino_conn_get_http_client(stmt->conn);
    }

    SQLRETURN ret = trino_resultset_fetch_scroll(rs, fetch_orientation,
                                                 (SQLROWID)fetch_offset, client);

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
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->resultset)
        return SQL_ERROR;

    trino_resultset_t *rs = (trino_resultset_t *)stmt->resultset;

    SQLRETURN ret =
        trino_resultset_get_data(rs, column_number, target_type, target_value,
                                 (SQLBUFFER_LENGTH)buffer_length, str_len_or_ind);

    /* Surface a truncation warning via SQLSTATE 01004 (data truncated). */
    if (ret == SQL_SUCCESS_WITH_INFO) {
        trino_diag_set_error(&stmt->diagnostics, "01004", 0,
                             "String or binary data, right-truncated");
    }
    return ret;
}

/* Copy a NUL-terminated string into a character output buffer, truncating if
 * necessary. *str_len_or_ind receives the full (untruncated) length. Returns
 * SQL_SUCCESS, or SQL_SUCCESS_WITH_INFO when truncated. */
static SQLRETURN copy_string_out(const char *value, SQLPOINTER buffer,
                                 SQLBUFFER_LENGTH buffer_length, SQLLEN *str_len_or_ind)
{
    size_t val_len = strlen(value);
    if (str_len_or_ind)
        *str_len_or_ind = (SQLLEN)val_len;

    if (!buffer || buffer_length <= 0) {
        return SQL_SUCCESS_WITH_INFO; /* length returned; no room to copy */
    }
    if (val_len >= (size_t)buffer_length) {
        memcpy(buffer, value, (size_t)buffer_length - 1);
        ((char *)buffer)[buffer_length - 1] = '\0';
        return SQL_SUCCESS_WITH_INFO;
    }
    memcpy(buffer, value, val_len + 1);
    return SQL_SUCCESS;
}

/* Convert a UTF-8 cell value to UTF-16 and copy into a wide-character output
 * buffer. Per ODBC, buffer_length and *str_len_or_ind for SQL_C_WCHAR are in
 * bytes. The full (untruncated) length in bytes is reported; truncation occurs
 * on a wide-character boundary and the buffer is always NUL-terminated.
 * Returns SQL_SUCCESS, SQL_SUCCESS_WITH_INFO on truncation, or SQL_ERROR. */
static SQLRETURN copy_wstring_out(const char *value, SQLPOINTER buffer,
                                  SQLBUFFER_LENGTH buffer_length, SQLLEN *str_len_or_ind)
{
    size_t wlen = 0;
    SQLWCHAR *wstr = trino_utf8_to_wchars(value, &wlen);
    if (!wstr)
        return SQL_ERROR;

    /* Full length, in bytes, excluding the NUL terminator. */
    if (str_len_or_ind)
        *str_len_or_ind = (SQLLEN)(wlen * sizeof(SQLWCHAR));

    if (!buffer || buffer_length <= 0) {
        free(wstr);
        return SQL_SUCCESS_WITH_INFO;
    }

    /* Number of whole wide chars that fit, reserving one for the terminator. */
    size_t max_wchars = (size_t)buffer_length / sizeof(SQLWCHAR);
    SQLWCHAR *out = (SQLWCHAR *)buffer;
    SQLRETURN ret = SQL_SUCCESS;

    if (max_wchars == 0) {
        /* Not even room for a terminator. */
        free(wstr);
        return SQL_SUCCESS_WITH_INFO;
    }

    size_t copy = wlen;
    if (copy > max_wchars - 1) {
        copy = max_wchars - 1;
        ret = SQL_SUCCESS_WITH_INFO;
    }
    memcpy(out, wstr, copy * sizeof(SQLWCHAR));
    out[copy] = 0;

    free(wstr);
    return ret;
}

/* Copy raw bytes into a binary output buffer, truncating if necessary. */
static SQLRETURN copy_binary_out(const char *value, SQLPOINTER buffer,
                                 SQLBUFFER_LENGTH buffer_length, SQLLEN *str_len_or_ind)
{
    size_t val_len = strlen(value);
    if (str_len_or_ind)
        *str_len_or_ind = (SQLLEN)val_len;

    if (!buffer || buffer_length <= 0) {
        return SQL_SUCCESS_WITH_INFO;
    }
    if (val_len > (size_t)buffer_length) {
        memcpy(buffer, value, (size_t)buffer_length);
        return SQL_SUCCESS_WITH_INFO;
    }
    memcpy(buffer, value, val_len);
    return SQL_SUCCESS;
}

/* Parse a signed integer from a Trino string value. On success stores the
 * value in *out and returns true. Returns false on a non-numeric value or
 * out-of-range result. */
static bool parse_int64(const char *value, long long *out)
{
    errno = 0;
    char *end = NULL;
    long long v = strtoll(value, &end, 10);
    if (end == value || errno == ERANGE)
        return false;
    /* Trailing whitespace is acceptable; other trailing chars are not. */
    while (*end && isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;
    *out = v;
    return true;
}

static bool parse_double(const char *value, double *out)
{
    errno = 0;
    char *end = NULL;
    double v = strtod(value, &end);
    if (end == value)
        return false;
    while (*end && isspace((unsigned char)*end))
        end++;
    if (*end != '\0')
        return false;
    *out = v;
    return true;
}

/* Parse up to `max` non-negative integer components from `value`, where each
 * component is followed by one of the characters in `seps` (and the final one
 * by NUL or any separator). Writes the parsed components into out[] and the
 * count into *count. Returns false if fewer than `min` components parse or a
 * component is non-numeric. Used for date/time string parsing without sscanf. */
static bool parse_components(const char *value, const char *seps, int min, int max,
                             long *out, int *count)
{
    int n = 0;
    const char *p = value;
    while (n < max) {
        errno = 0;
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p || errno != 0 || v < 0)
            break;
        out[n++] = v;
        if (*end == '\0')
            break;
        if (!strchr(seps, *end))
            break; /* unexpected separator: stop here */
        p = end + 1;
    }
    *count = n;
    return n >= min;
}

/* Parse a decimal string (e.g. "-123.45") into a SQL_NUMERIC_STRUCT: sign,
 * scale (fractional digit count), precision (significant digit count) and a
 * 16-byte little-endian unscaled mantissa. Returns false on malformed input or
 * if the mantissa exceeds 16 bytes. */
static bool parse_numeric(const char *value, SQL_NUMERIC_STRUCT *out)
{
    const char *p = value;
    while (*p == ' ' || *p == '\t')
        p++;

    unsigned char sign = 1; /* 1 = positive, 0 = negative */
    if (*p == '+') {
        p++;
    } else if (*p == '-') {
        sign = 0;
        p++;
    }

    unsigned char mant[SQL_MAX_NUMERIC_LEN] = {0};
    int digits = 0; /* total significant digits accumulated */
    int scale = 0;  /* digits after the decimal point */
    bool seen_dot = false;
    bool any = false;

    for (; *p; p++) {
        if (*p == '.') {
            if (seen_dot)
                return false; /* two decimal points */
            seen_dot = true;
            continue;
        }
        if (*p < '0' || *p > '9') {
            /* Allow trailing whitespace only. */
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p != '\0')
                return false;
            break;
        }
        any = true;

        /* mant = mant * 10 + digit, as a little-endian byte big-integer. */
        unsigned int carry = (unsigned int)(*p - '0');
        for (int i = 0; i < SQL_MAX_NUMERIC_LEN; i++) {
            unsigned int prod = (unsigned int)mant[i] * 10u + carry;
            mant[i] = (unsigned char)(prod & 0xFF);
            carry = prod >> 8;
        }
        if (carry != 0)
            return false; /* overflow beyond 16 bytes */

        digits++;
        if (seen_dot)
            scale++;
    }

    if (!any)
        return false;

    out->sign = sign;
    out->scale = (SQLSCHAR)scale;
    /* Precision is the count of significant digits (at least 1). */
    out->precision = (SQLCHAR)(digits > 0 ? digits : 1);
    memcpy(out->val, mant, SQL_MAX_NUMERIC_LEN);
    return true;
}

SQLRETURN trino_resultset_get_data(trino_resultset_t *rs, SQLUSMALLINT col,
                                   SQLSMALLINT C_type, SQLPOINTER buffer,
                                   SQLBUFFER_LENGTH buffer_length, SQLLEN *str_len_or_ind)
{
    if (!rs || !rs->query_results)
        return SQL_ERROR;

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

    /* SQL NULL: report via the indicator (which is mandatory for nullable
     * data). Without an indicator the application cannot represent NULL. */
    if (!value) {
        if (str_len_or_ind) {
            *str_len_or_ind = SQL_NULL_DATA;
            return SQL_SUCCESS;
        }
        return SQL_ERROR; /* 22002: indicator required but not provided */
    }

    /* Convert based on requested C type. */
    switch (C_type) {
        case SQL_C_CHAR:
            return copy_string_out(value, buffer, buffer_length, str_len_or_ind);

        case SQL_C_WCHAR:
            return copy_wstring_out(value, buffer, buffer_length, str_len_or_ind);

        case SQL_C_BINARY:
            return copy_binary_out(value, buffer, buffer_length, str_len_or_ind);

        case SQL_C_BIT: {
            if (!buffer)
                return SQL_ERROR;
            /* Accept 0/1 and true/false. */
            unsigned char b;
            if (strcasecmp(value, "true") == 0 || strcmp(value, "1") == 0)
                b = 1;
            else
                b = 0;
            *(unsigned char *)buffer = b;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(unsigned char);
            return SQL_SUCCESS;
        }

        case SQL_C_STINYINT:
        case SQL_C_TINYINT: {
            long long v;
            if (!buffer || !parse_int64(value, &v) || v < SCHAR_MIN || v > SCHAR_MAX)
                return SQL_ERROR;
            *(signed char *)buffer = (signed char)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(signed char);
            return SQL_SUCCESS;
        }
        case SQL_C_UTINYINT: {
            long long v;
            if (!buffer || !parse_int64(value, &v) || v < 0 || v > UCHAR_MAX)
                return SQL_ERROR;
            *(unsigned char *)buffer = (unsigned char)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(unsigned char);
            return SQL_SUCCESS;
        }
        case SQL_C_SSHORT:
        case SQL_C_SHORT: {
            long long v;
            if (!buffer || !parse_int64(value, &v) || v < SHRT_MIN || v > SHRT_MAX)
                return SQL_ERROR;
            *(short *)buffer = (short)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(short);
            return SQL_SUCCESS;
        }
        case SQL_C_USHORT: {
            long long v;
            if (!buffer || !parse_int64(value, &v) || v < 0 || v > USHRT_MAX)
                return SQL_ERROR;
            *(unsigned short *)buffer = (unsigned short)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(unsigned short);
            return SQL_SUCCESS;
        }
        case SQL_C_SLONG:
        case SQL_C_LONG: {
            long long v;
            if (!buffer || !parse_int64(value, &v) || v < INT32_MIN || v > INT32_MAX)
                return SQL_ERROR;
            *(SQLINTEGER *)buffer = (SQLINTEGER)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQLINTEGER);
            return SQL_SUCCESS;
        }
        case SQL_C_ULONG: {
            long long v;
            if (!buffer || !parse_int64(value, &v) || v < 0 || v > (long long)UINT32_MAX)
                return SQL_ERROR;
            *(SQLUINTEGER *)buffer = (SQLUINTEGER)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQLUINTEGER);
            return SQL_SUCCESS;
        }
        case SQL_C_SBIGINT: {
            long long v;
            if (!buffer || !parse_int64(value, &v))
                return SQL_ERROR;
            *(SQLBIGINT *)buffer = (SQLBIGINT)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQLBIGINT);
            return SQL_SUCCESS;
        }
        case SQL_C_UBIGINT: {
            errno = 0;
            char *end = NULL;
            unsigned long long v = strtoull(value, &end, 10);
            if (!buffer || end == value || errno == ERANGE)
                return SQL_ERROR;
            while (*end && isspace((unsigned char)*end))
                end++;
            if (*end != '\0')
                return SQL_ERROR;
            *(SQLUBIGINT *)buffer = (SQLUBIGINT)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQLUBIGINT);
            return SQL_SUCCESS;
        }

        case SQL_C_FLOAT: {
            double v;
            if (!buffer || !parse_double(value, &v))
                return SQL_ERROR;
            *(SQLREAL *)buffer = (SQLREAL)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQLREAL);
            return SQL_SUCCESS;
        }
        case SQL_C_DOUBLE: {
            double v;
            if (!buffer || !parse_double(value, &v))
                return SQL_ERROR;
            *(SQLDOUBLE *)buffer = (SQLDOUBLE)v;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQLDOUBLE);
            return SQL_SUCCESS;
        }

        case SQL_C_NUMERIC: {
            if (!buffer)
                return SQL_ERROR;
            SQL_NUMERIC_STRUCT num = {0};
            if (!parse_numeric(value, &num))
                return SQL_ERROR;
            *(SQL_NUMERIC_STRUCT *)buffer = num;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQL_NUMERIC_STRUCT);
            return SQL_SUCCESS;
        }

        case SQL_C_TYPE_DATE:
        case SQL_C_DATE: {
            /* Expect "YYYY-MM-DD". */
            if (!buffer)
                return SQL_ERROR;
            long c[3];
            int n = 0;
            if (!parse_components(value, "-", 3, 3, c, &n))
                return SQL_ERROR;
            SQL_DATE_STRUCT d = {0};
            d.year = (SQLSMALLINT)c[0];
            d.month = (SQLUSMALLINT)c[1];
            d.day = (SQLUSMALLINT)c[2];
            *(SQL_DATE_STRUCT *)buffer = d;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQL_DATE_STRUCT);
            return SQL_SUCCESS;
        }
        case SQL_C_TYPE_TIME:
        case SQL_C_TIME: {
            /* Expect "HH:MM:SS" (fractional seconds, if any, are ignored). */
            if (!buffer)
                return SQL_ERROR;
            long c[3];
            int n = 0;
            if (!parse_components(value, ":", 3, 3, c, &n))
                return SQL_ERROR;
            SQL_TIME_STRUCT t = {0};
            t.hour = (SQLUSMALLINT)c[0];
            t.minute = (SQLUSMALLINT)c[1];
            t.second = (SQLUSMALLINT)c[2];
            *(SQL_TIME_STRUCT *)buffer = t;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQL_TIME_STRUCT);
            return SQL_SUCCESS;
        }
        case SQL_C_TYPE_TIMESTAMP:
        case SQL_C_TIMESTAMP: {
            /* Expect "YYYY-MM-DD HH:MM:SS[.ffffff]" (space or 'T' separator). */
            if (!buffer)
                return SQL_ERROR;
            /* Components: year, month, day, hour, minute, second, [fraction].
             * Separators between them are '-', ' '/'T', ':' and '.'. */
            long c[7] = {0};
            int n = 0;
            if (!parse_components(value, "-T :.", 6, 7, c, &n))
                return SQL_ERROR;
            long frac = (n >= 7) ? c[6] : 0;
            SQL_TIMESTAMP_STRUCT ts = {0};
            ts.year = (SQLSMALLINT)c[0];
            ts.month = (SQLUSMALLINT)c[1];
            ts.day = (SQLUSMALLINT)c[2];
            ts.hour = (SQLUSMALLINT)c[3];
            ts.minute = (SQLUSMALLINT)c[4];
            ts.second = (SQLUSMALLINT)c[5];
            /* SQL fraction is in nanoseconds; the source value is microseconds. */
            ts.fraction = (SQLUINTEGER)(frac * 1000);
            *(SQL_TIMESTAMP_STRUCT *)buffer = ts;
            if (str_len_or_ind)
                *str_len_or_ind = (SQLLEN)sizeof(SQL_TIMESTAMP_STRUCT);
            return SQL_SUCCESS;
        }

        default:
            /* Unknown target type: fall back to a string copy. */
            return copy_string_out(value, buffer, buffer_length, str_len_or_ind);
    }
}

/* ========================================================================
 * SQLBindCol
 * ======================================================================== */

SQLRETURN SQLBindCol(SQLHSTMT statement_handle, SQLUSMALLINT column_number,
                     SQLSMALLINT target_type, SQLPOINTER target_value,
                     SQLLEN buffer_length, SQLLEN *str_len_or_ind)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->ard)
        return SQL_ERROR;

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
