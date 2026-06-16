/* Wide character string helpers for Trino ODBC driver */

#include "trino_odbc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Convert SQLWCHAR (UTF-16) to UTF-8 string. Caller must free result. */
char *trino_wchars_to_utf8(const SQLWCHAR *wstr, size_t wlen)
{
    if (!wstr || wlen == 0) {
        return NULL;
    }

#ifdef _WIN32
    /* Windows: use WideCharToMultiByte for UTF-16 to UTF-8 conversion */
    int required_len =
        WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)wstr, (int)wlen, NULL, 0, NULL, NULL);
    if (required_len <= 0) {
        return NULL;
    }

    char *result = malloc(required_len + 1);
    if (!result) {
        return NULL;
    }

    WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)wstr, (int)wlen, result, required_len, NULL,
                        NULL);
    result[required_len] = '\0';

    return result;
#else
    /* Non-Windows: simple UTF-16 to UTF-8 conversion */
    size_t utf8_len = 0;
    const SQLWCHAR *p = wstr;
    size_t i;

    /* First pass: calculate required UTF-8 length */
    for (i = 0; i < wlen && p[i] != 0; i++) {
        SQLWCHAR ch = p[i];

        if (ch < 0x80) {
            utf8_len += 1;
        } else if (ch < 0x800) {
            utf8_len += 2;
        } else {
            utf8_len += 3;
        }
    }

    /* Handle surrogate pairs */
    for (i = 0; i < wlen - 1; i++) {
        if (p[i] >= 0xD800 && p[i] <= 0xDBFF && p[i + 1] >= 0xDC00 &&
            p[i + 1] <= 0xDFFF) {
            utf8_len += 1; /* Surrogate pairs need extra byte */
        }
    }

    char *result = malloc(utf8_len + 1);
    if (!result) {
        return NULL;
    }

    /* Second pass: convert to UTF-8 */
    size_t pos = 0;
    for (i = 0; i < wlen && p[i] != 0; i++) {
        SQLWCHAR ch = p[i];

        /* Check for surrogate pair */
        if (ch >= 0xD800 && ch <= 0xDBFF && i + 1 < wlen && p[i + 1] >= 0xDC00 &&
            p[i + 1] <= 0xDFFF) {
            /* Combine surrogate pair into Unicode code point */
            uint32_t codepoint = 0x10000 + ((ch & 0x3FF) << 10) + (p[++i] & 0x3FF);

            /* Encode as UTF-8 (4 bytes) */
            result[pos++] = 0xF0 | (codepoint >> 18);
            result[pos++] = 0x80 | ((codepoint >> 12) & 0x3F);
            result[pos++] = 0x80 | ((codepoint >> 6) & 0x3F);
            result[pos++] = 0x80 | (codepoint & 0x3F);
        } else if (ch < 0x80) {
            /* 1-byte UTF-8 */
            result[pos++] = (char)ch;
        } else if (ch < 0x800) {
            /* 2-byte UTF-8 */
            result[pos++] = 0xC0 | (ch >> 6);
            result[pos++] = 0x80 | (ch & 0x3F);
        } else {
            /* 3-byte UTF-8 */
            result[pos++] = 0xE0 | (ch >> 12);
            result[pos++] = 0x80 | ((ch >> 6) & 0x3F);
            result[pos++] = 0x80 | (ch & 0x3F);
        }
    }

    result[pos] = '\0';
    return result;
#endif
}

/* Convert UTF-8 to SQLWCHAR (UTF-16). Caller must free result. */
SQLWCHAR *trino_utf8_to_wchars(const char *str, size_t *out_wlen)
{
    if (!str || !out_wlen) {
        return NULL;
    }

    size_t len = strlen(str);

#ifdef _WIN32
    /* Windows: use MultiByteToWideChar for UTF-8 to UTF-16 conversion */
    int required_len = MultiByteToWideChar(CP_UTF8, 0, str, (int)len, NULL, 0);
    if (required_len <= 0) {
        return NULL;
    }

    SQLWCHAR *result = malloc((required_len + 1) * sizeof(SQLWCHAR));
    if (!result) {
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, str, (int)len, (LPWSTR)result, required_len);
    result[required_len] = 0;

    *out_wlen = (size_t)required_len;
    return result;
#else
    /* Non-Windows: simple UTF-8 to UTF-16 conversion */
    size_t wlen = 0;
    const char *p = str;
    size_t i;

    for (i = 0; i < len;) {
        unsigned char c = (unsigned char)p[i];

        if (c < 0x80) {
            /* 1-byte UTF-8 */
            wlen++;
            i += 1;
        } else if (c < 0xE0) {
            /* 2-byte UTF-8 */
            wlen++;
            i += 2;
        } else if (c < 0xF0) {
            /* 3-byte UTF-8 */
            wlen++;
            i += 3;
        } else {
            /* 4-byte UTF-8 - becomes surrogate pair (2 wide chars) */
            wlen += 2;
            i += 4;
        }
    }

    SQLWCHAR *result = malloc((wlen + 1) * sizeof(SQLWCHAR));
    if (!result) {
        return NULL;
    }

    /* Convert UTF-8 to UTF-16 */
    size_t pos = 0;
    for (i = 0; i < len;) {
        unsigned char c = (unsigned char)p[i];

        if (c < 0x80) {
            result[pos++] = (SQLWCHAR)c;
            i += 1;
        } else if (c < 0xE0) {
            /* 2-byte UTF-8 */
            uint32_t codepoint =
                ((uint32_t)(c & 0x1F) << 6) | ((uint32_t)p[i + 1] & 0x3F);
            result[pos++] = (SQLWCHAR)codepoint;
            i += 2;
        } else if (c < 0xF0) {
            /* 3-byte UTF-8 */
            uint32_t codepoint = ((uint32_t)(c & 0x0F) << 12) |
                                 ((uint32_t)(p[i + 1] & 0x3F) << 6) |
                                 ((uint32_t)p[i + 2] & 0x3F);
            result[pos++] = (SQLWCHAR)codepoint;
            i += 3;
        } else {
            /* 4-byte UTF-8 - encode as surrogate pair */
            uint32_t codepoint =
                ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(p[i + 1] & 0x3F) << 12) |
                ((uint32_t)(p[i + 2] & 0x3F) << 6) | ((uint32_t)p[i + 3] & 0x3F);

            codepoint -= 0x10000;
            result[pos++] = (SQLWCHAR)(0xD800 | (codepoint >> 10));
            result[pos++] = (SQLWCHAR)(0xDC00 | (codepoint & 0x3FF));
            i += 4;
        }
    }

    result[pos] = 0;
    *out_wlen = pos;

    return result;
#endif
}

/* Get length of UTF-16 string (in wide chars) */
size_t trino_wstrlen(const SQLWCHAR *wstr)
{
    if (!wstr) {
        return 0;
    }

    size_t len = 0;
    while (wstr[len] != 0) {
        len++;
    }
    return len;
}
