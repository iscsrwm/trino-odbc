#ifndef TRINO_ODBC_CATALOG_H
#define TRINO_ODBC_CATALOG_H

#include "trino_odbc/core.h"

/* ========================================================================
 * Catalog functions - SQLTables, SQLColumns, SQLPrimaryKeys, SQLForeignKeys
 *
 * Signatures match the standard ODBC catalog API (non-const SQLCHAR* name
 * arguments with SQLSMALLINT name lengths). Constants such as SQL_BEST_ROWID,
 * SQL_ROWVER and SQL_SCOPE_* are provided by <sqlext.h>.
 * ======================================================================== */

SQLRETURN SQLTables(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                    SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                    SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                    SQLSMALLINT table_pattern_length, SQLCHAR *types,
                    SQLSMALLINT types_length);

SQLRETURN SQLColumns(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                     SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                     SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                     SQLSMALLINT table_pattern_length, SQLCHAR *column_pattern,
                     SQLSMALLINT column_pattern_length);

SQLRETURN SQLPrimaryKeys(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                         SQLSMALLINT catalog_name_length, SQLCHAR *schema_name,
                         SQLSMALLINT schema_name_length, SQLCHAR *table_name,
                         SQLSMALLINT table_name_length);

SQLRETURN SQLForeignKeys(SQLHSTMT statement_handle, SQLCHAR *pk_catalog,
                         SQLSMALLINT pk_catalog_length, SQLCHAR *pk_schema,
                         SQLSMALLINT pk_schema_length, SQLCHAR *pk_table,
                         SQLSMALLINT pk_table_length, SQLCHAR *fk_catalog,
                         SQLSMALLINT fk_catalog_length, SQLCHAR *fk_schema,
                         SQLSMALLINT fk_schema_length, SQLCHAR *fk_table,
                         SQLSMALLINT fk_table_length);

SQLRETURN SQLTablePrivileges(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                             SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                             SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                             SQLSMALLINT table_pattern_length);

SQLRETURN SQLColumnPrivileges(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                              SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                              SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                              SQLSMALLINT table_pattern_length, SQLCHAR *column_pattern,
                              SQLSMALLINT column_pattern_length);

SQLRETURN SQLSpecialColumns(SQLHSTMT statement_handle, SQLUSMALLINT identifier_type,
                            SQLCHAR *catalog_name, SQLSMALLINT catalog_name_length,
                            SQLCHAR *schema_name, SQLSMALLINT schema_name_length,
                            SQLCHAR *table_name, SQLSMALLINT table_name_length,
                            SQLUSMALLINT identifier_scope, SQLUSMALLINT nullable);

/* ========================================================================
 * Statistics and data source info
 * ======================================================================== */

SQLRETURN SQLStatistics(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                        SQLSMALLINT catalog_name_length, SQLCHAR *schema_name,
                        SQLSMALLINT schema_name_length, SQLCHAR *table_name,
                        SQLSMALLINT table_name_length, SQLUSMALLINT unique,
                        SQLUSMALLINT reserved);

SQLRETURN SQLDataSources(SQLHENV environment_handle, SQLUSMALLINT direction,
                         SQLCHAR *server_name, SQLSMALLINT buffer_length,
                         SQLSMALLINT *name_length_ptr, SQLCHAR *description,
                         SQLSMALLINT description_buffer_length,
                         SQLSMALLINT *description_length_ptr);

SQLRETURN SQLDrivers(SQLHENV environment_handle, SQLUSMALLINT direction,
                     SQLCHAR *driver_description, SQLSMALLINT buffer_length,
                     SQLSMALLINT *description_length_ptr, SQLCHAR *driver_attributes,
                     SQLSMALLINT attributes_buffer_length,
                     SQLSMALLINT *attributes_length_ptr);

#endif /* TRINO_ODBC_CATALOG_H */
