/* Lightweight diagnostic logging for the Trino ODBC driver.
 *
 * Enabled at runtime by setting the TRINO_ODBC_LOG environment variable to a
 * writable file path (e.g. C:\Temp\trino_odbc.log). When unset, logging is a
 * no-op with negligible overhead. This exists so connection failures can be
 * diagnosed from inside the driver, independent of the ODBC Driver Manager's
 * own tracing (which only fires once the DM successfully dispatches into the
 * driver).
 */

#include "trino_odbc/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>

void trino_log(const char *fmt, ...)
{
    const char *path = getenv("TRINO_ODBC_LOG");
    if (!path || !*path)
        return;

    FILE *f = fopen(path, "a");
    if (!f)
        return;

    /* Timestamp prefix. */
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _WIN32
    localtime_s(&tmv, &now);
#else
    localtime_r(&now, &tmv);
#endif
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] ", ts);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);

    fputc('\n', f);
    fclose(f);
}
