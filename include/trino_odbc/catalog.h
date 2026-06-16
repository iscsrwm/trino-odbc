#ifndef TRINO_ODBC_CATALOG_H
#define TRINO_ODBC_CATALOG_H

#include "trino_odbc/core.h"

/* ========================================================================
 * Catalog functions - SQLTables, SQLColumns, SQLPrimaryKeys, SQLForeignKeys
 * ======================================================================== */

SQLRETURN SQLTables(SQLHSTMT statement_handle,
                    const SQLCHAR *catalog_name, SQLULEN catalog_name_length,
                    const SQLCHAR *schema_pattern, SQLULEN schema_pattern_length,
                    const SQLCHAR *table_pattern, SQLULEN table_pattern_length,
                    const SQLCHAR *types, SQLULEN types_length);

SQLRETURN SQLColumns(SQLHSTMT statement_handle,
                     const SQLCHAR *catalog_name, SQLULEN catalog_name_length,
                     const SQLCHAR *schema_pattern, SQLULEN schema_pattern_length,
                     const SQLCHAR *table_pattern, SQLULEN table_pattern_length,
                     const SQLCHAR *column_pattern, SQLULEN column_pattern_length);

SQLRETURN SQLPrimaryKeys(SQLHSTMT statement_handle,
                         const SQLCHAR *catalog_name, SQLULEN catalog_name_length,
                         const SQLCHAR *schema_name, SQLULEN schema_name_length,
                         const SQLCHAR *table_name, SQLULEN table_name_length);

SQLRETURN SQLForeignKeys(SQLHSTMT statement_handle,
                         const SQLCHAR *pk_catalog, SQLULEN pk_catalog_length,
                         const SQLCHAR *pk_schema, SQLULEN pk_schema_length,
                         const SQLCHAR *pk_table, SQLULEN pk_table_length,
                         const SQLCHAR *fk_catalog, SQLULEN fk_catalog_length,
                         const SQLCHAR *fk_schema, SQLULEN fk_schema_length,
                         const SQLCHAR *fk_table, SQLULEN fk_table_length);

SQLRETURN SQLGetTablePrivileges(SQLHSTMT statement_handle,
                                const SQLCHAR *catalog_name, SQLULEN catalog_name_length,
                                const SQLCHAR *schema_pattern, SQLULEN schema_pattern_length,
                                const SQLCHAR *table_pattern, SQLULEN table_pattern_length);

SQLRETURN SQLGetColumnPrivileges(SQLHSTMT statement_handle,
                                 const SQLCHAR *catalog_name, SQLULEN catalog_name_length,
                                 const SQLCHAR *schema_pattern, SQLULEN schema_pattern_length,
                                 const SQLCHAR *table_pattern, SQLULEN table_pattern_length,
                                 const SQLCHAR *column_pattern, SQLULEN column_pattern_length);

SQLRETURN SQLSpecialColumns(SQLHSTMT statement_handle, SQLUSMALLINT identifier_type,
                            const SQLCHAR *catalog_name, SQLULEN catalog_name_length,
                            const SQLCHAR *schema_name, SQLULEN schema_name_length,
                            const SQLCHAR *table_name, SQLULEN table_name_length,
                            SQLUSMALLINT identifier_scope, SQLUSMALLINT nullable);

/* ========================================================================
 * Statistics and data source info
 * ======================================================================== */

/* Special column identifiers */
#define SQL_BEST_ROWID    1
#define SQL_ROWVER        2
#define SQL_ROW_UNSAFETY  0
#define SQL_ROW_LOC       1
#define SQL_ROW_UPDATABLE 2

SQLRETURN SQLStatistics(SQLHSTMT statement_handle,
                        const SQLCHAR *catalog_name, SQLULEN catalog_name_length,
                        const SQLCHAR *schema_name, SQLULEN schema_name_length,
                        const SQLCHAR *table_name, SQLULEN table_name_length,
                        SQLUSMALLINT unique, SQLUSMALLINT nullable);

SQLRETURN SQLDataSources(SQLHENV environment_handle, SQLUSMALLINT direction,
                         const SQLCHAR *connection_string, SQLSMALLINT connection_string_length,
                         SQLCHAR *server_name, SQLSMALLINT buffer_length,
                         SQLSMALLINT *name_length_ptr,
                         SQLCHAR *driver_name, SQLSMALLINT driver_name_buffer_length,
                         SQLSMALLINT *driver_name_length_ptr);

SQLRETURN SQLDrivers(SQLHENV environment_handle, SQLUSMALLINT driver_completion,
                     SQLCHAR *driver_data, SQLSMALLINT buffer_length,
                     SQLSMALLINT *str_length_ptr,
                     SQLCHAR *driver_attributes, SQLSMALLINT attributes_buffer_length,
                     SQLSMALLINT *attributes_length_ptr);

#endif /* TRINO_ODBC_CATALOG_H */
