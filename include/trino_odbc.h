#ifndef TRINO_ODBC_H
#define TRINO_ODBC_H

/*
 * Public ABI for the Trino ODBC driver.
 *
 * This header intentionally builds on the standard ODBC headers provided by
 * the driver manager (unixODBC / iODBC on POSIX, the Windows SDK on Windows)
 * rather than re-declaring the ODBC types and constants. Re-declaring them is
 * error-prone (return codes and SQL_C_* type values must match the driver
 * manager exactly) and previously caused incompatible values such as
 * SQL_ERROR == SQL_NO_DATA.
 *
 * Only genuinely project-specific declarations live here:
 *   - a handful of convenience type aliases the codebase relies on that are
 *     not part of the standard ODBC headers,
 *   - Trino-specific SQLGetInfo / attribute extension codes,
 *   - the driver version string,
 *   - wide-character helper declarations implemented in protocol/wchar.c.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Standard ODBC headers from the driver manager. */
#include <sql.h>
#include <sqlext.h>
#include <sqltypes.h>
#include <sqlucode.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Project-specific type aliases
 *
 * These are not declared by the standard ODBC headers but are used
 * throughout this codebase. They are defined in terms of the standard
 * fixed-width / ODBC types so they remain ABI-compatible.
 * ======================================================================== */

typedef unsigned char  SQLBOOLEAN;
typedef SQLUSMALLINT   SQLUWORD;
typedef SQLSMALLINT    SQLSWORD;
typedef SQLINTEGER     SQLLONG;
typedef SQLUINTEGER    SQLULONG;
typedef signed char    SQLBYTE;
typedef unsigned char  SQLUBYTE;
typedef short          SQLSHORT;
typedef unsigned short SQLUSHORT;

/* Length type used for SQLColAttribute-style buffer lengths in this codebase. */
typedef SQLLEN         SQLBUFFER_LENGTH;

/* Row identifier used by the internal result-set cursor helpers. */
typedef SQLLEN         SQLROWID;

#ifndef SQL_TRUE
#define SQL_TRUE  1
#endif
#ifndef SQL_FALSE
#define SQL_FALSE 0
#endif

/* ========================================================================
 * Wide character helpers (implemented in src/protocol/wchar.c)
 * ======================================================================== */

/* Convert SQLWCHAR (UTF-16) to UTF-8 string. Caller must free result. */
char *trino_wchars_to_utf8(const SQLWCHAR *wstr, size_t wlen);

/* Convert UTF-8 to SQLWCHAR (UTF-16). Caller must free result. */
SQLWCHAR *trino_utf8_to_wchars(const char *str, size_t *out_wlen);

/* Get length of UTF-16 string (in wide chars). */
size_t trino_wstrlen(const SQLWCHAR *wstr);

/* ========================================================================
 * Internal buffer sizes
 *
 * Standard ODBC defines SQL_MAX_IDENTIFIER_LEN / SQL_MAX_MESSAGE_LEN with
 * values intended as reported limits (e.g. 10005), which are unsuitable as
 * fixed in-struct buffer sizes. These project-specific constants size the
 * driver's internal fixed buffers and preserve the original layout intent.
 * ======================================================================== */

#define TRINO_MAX_IDENTIFIER_LEN 128
#define TRINO_MAX_MESSAGE_LEN    512

/* ========================================================================
 * Driver version
 * ======================================================================== */

#define TRINO_ODBC_VERSION_STR "1.0.0"

/* ========================================================================
 * Trino-specific extensions
 *
 * Custom SQLGetInfo / attribute codes in the driver-reserved range used to
 * expose Trino query metadata to applications.
 * ======================================================================== */

#define SQL_TRINO_QUERY_ID              12001
#define SQL_TRINO_QUERY_STATE           12002
#define SQL_TRINO_QUERY_ELAPSED_TIME    12003
#define SQL_TRINO_QUERY_ROWS_PROCESSED  12004
#define SQL_TRINO_QUERY_BYTES_PROCESSED 12005

#ifdef __cplusplus
}
#endif

#endif /* TRINO_ODBC_H */
