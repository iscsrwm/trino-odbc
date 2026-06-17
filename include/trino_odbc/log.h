#ifndef TRINO_ODBC_LOG_H
#define TRINO_ODBC_LOG_H

/* Append a formatted line to the file named by the TRINO_ODBC_LOG environment
 * variable. No-op when the variable is unset. printf-style format. */
#if defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void trino_log(const char *fmt, ...);

#endif /* TRINO_ODBC_LOG_H */
