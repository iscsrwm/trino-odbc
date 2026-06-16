#include "trino_odbc/catalog.h"
#include "trino_odbc/statement.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define TRINO_CATALOG_BUF_SIZE 4096

/* ========================================================================
 * Helper: build pattern string for LIKE clause
 * ======================================================================== */

static void build_pattern(const SQLCHAR *pattern, SQLSMALLINT pattern_len, char *out,
                          size_t out_size)
{
    /* Resolve null-terminated strings to their actual length. */
    if (pattern && pattern_len == SQL_NTS) {
        pattern_len = (SQLSMALLINT)strlen((const char *)pattern);
    }
    if (!pattern || pattern_len <= 0) {
        strcpy(out, "'%'");
    } else {
        size_t i = 0;
        out[i++] = '\'';
        size_t p = 0;
        size_t plen = (size_t)pattern_len;
        while (p < plen && i < out_size - 3) {
            if (pattern[p] == '_') {
                out[i++] = '\\';
                out[i++] = '_';
            } else if (pattern[p] == '%') {
                out[i++] = '\\';
                out[i++] = '%';
            } else {
                out[i++] = (char)pattern[p];
            }
            p++;
        }
        out[i++] = '%';
        out[i++] = '\'';
        out[i] = '\0';
    }
}

/* ========================================================================
 * Helper: get catalog/schema from connection or use default
 * ======================================================================== */

static const char *get_catalog_or_default(trino_conn_t *conn)
{
    if (conn && conn->catalog && strlen((char *)conn->catalog) > 0) {
        return (char *)conn->catalog;
    }
    return "memory";
}

/* ========================================================================
 * SQLTables
 * ========================================================================
 * Returns: TABLE_CAT, TABLE_SCHEM, TABLE_NAME, TABLE_TYPE, REMARKS
 * ======================================================================== */

SQLRETURN SQLTables(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                    SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                    SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                    SQLSMALLINT table_pattern_length, SQLCHAR *types,
                    SQLSMALLINT types_length)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char cat_pat[256], schema_pat[256], table_pat[256], types_pat[256];

    /* Build patterns */
    build_pattern(catalog_name, catalog_name_length, cat_pat, sizeof(cat_pat));
    build_pattern(schema_pattern, schema_pattern_length, schema_pat, sizeof(schema_pat));
    build_pattern(table_pattern, table_pattern_length, table_pat, sizeof(table_pat));

    /* Build types pattern */
    if (types && types_length == SQL_NTS) {
        types_length = (SQLSMALLINT)strlen((const char *)types);
    }
    if (!types || types_length <= 0) {
        strcpy(types_pat, "('TABLE','VIEW')");
    } else {
        /* Convert comma-separated types to SQL IN clause */
        size_t types_len = (size_t)types_length;
        size_t i = 0;
        types_pat[i++] = '(';
        size_t t = 0;
        bool first = true;
        while (t < types_len) {
            if (types[t] == ',') {
                t++;
                continue;
            }
            /* Find end of this type */
            size_t start = t;
            while (t < types_len && types[t] != ',')
                t++;
            if (!first)
                types_pat[i++] = ',';
            first = false;
            types_pat[i++] = '\'';
            size_t j = start;
            while (j < t && i < sizeof(types_pat) - 3) {
                types_pat[i++] = (char)types[j++];
            }
            types_pat[i++] = '\'';
        }
        types_pat[i++] = ')';
        types_pat[i] = '\0';
    }

    /* Construct SQL query against information_schema.tables */
    const char *catalog = get_catalog_or_default(stmt->conn);
    snprintf(sql, sizeof(sql),
             "SELECT TABLE_CATALOG AS TABLE_CAT, TABLE_SCHEMA AS TABLE_SCHEM, "
             "TABLE_NAME, TABLE_TYPE, '' AS REMARKS "
             "FROM %s.information_schema.tables "
             "WHERE TABLE_CATALOG LIKE %s "
             "AND TABLE_SCHEMA LIKE %s "
             "AND TABLE_NAME LIKE %s "
             "AND TABLE_TYPE IN %s "
             "ORDER BY TABLE_TYPE, TABLE_SCHEM, TABLE_NAME",
             catalog, cat_pat, schema_pat, table_pat, types_pat);

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLColumns
 * ========================================================================
 * Returns: TABLE_CAT, TABLE_SCHEM, TABLE_NAME, COLUMN_NAME, DATA_TYPE,
 *          TYPE_NAME, COLUMN_SIZE, BUFFER_LENGTH, DECIMAL_DIGITS,
 *          NUM_PREC_RADIX, NULLABLE, REMARKS, COLUMN_DEF, SQL_DATA_TYPE,
 *          SQL_DATETIME_SUB, CHAR_OCTET_LENGTH, ORDINAL_POSITION,
 *          IS_NULLABLE, SCOPE_CATALOG, SCOPE_SCHEMA, SCOPE_TABLE,
 *          DATA_TYPE, DATETIME_PRECISION, CHARACTER_SET_CATALOG,
 *          CHARACTER_SET_SCHEMA, CHARACTER_SET_NAME, COLLATION_CATALOG,
 *          COLLATION_SCHEMA, COLLATION_NAME
 * ======================================================================== */

SQLRETURN SQLColumns(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                     SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                     SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                     SQLSMALLINT table_pattern_length, SQLCHAR *column_pattern,
                     SQLSMALLINT column_pattern_length)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char cat_pat[256], schema_pat[256], table_pat[256], col_pat[256];

    build_pattern(catalog_name, catalog_name_length, cat_pat, sizeof(cat_pat));
    build_pattern(schema_pattern, schema_pattern_length, schema_pat, sizeof(schema_pat));
    build_pattern(table_pattern, table_pattern_length, table_pat, sizeof(table_pat));
    build_pattern(column_pattern, column_pattern_length, col_pat, sizeof(col_pat));

    const char *catalog = get_catalog_or_default(stmt->conn);
    snprintf(sql, sizeof(sql),
             "SELECT TABLE_CATALOG AS TABLE_CAT, TABLE_SCHEMA AS TABLE_SCHEM, "
             "TABLE_NAME, COLUMN_NAME, DATA_TYPE AS TYPE_NAME, "
             "CASE DATA_TYPE "
             "  WHEN 'varchar' THEN 12 WHEN 'char' THEN 1 "
             "  WHEN 'smallint' THEN 5 WHEN 'integer' THEN 4 WHEN 'bigint' THEN -5 "
             "  WHEN 'real' THEN 6 WHEN 'double' THEN 8 "
             "  WHEN 'boolean' THEN -7 WHEN 'varbinary' THEN -2 "
             "  WHEN 'date' THEN 91 WHEN 'time' THEN 92 WHEN 'timestamp' THEN 93 "
             "  WHEN 'json' THEN -1 WHEN 'array' THEN 2003 WHEN 'map' THEN 2004 "
             "  ELSE 11 END AS DATA_TYPE, "
             "COALESCE(CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, 0) AS COLUMN_SIZE, "
             "0 AS BUFFER_LENGTH, "
             "COALESCE(NUMERIC_SCALE, 0) AS DECIMAL_DIGITS, "
             "10 AS NUM_PREC_RADIX, "
             "CASE WHEN IS_NULLABLE = 'YES' THEN 1 ELSE 0 END AS NULLABLE, "
             "'' AS REMARKS, NULL AS COLUMN_DEF, "
             "CASE DATA_TYPE "
             "  WHEN 'varchar' THEN 12 WHEN 'char' THEN 1 "
             "  WHEN 'smallint' THEN 5 WHEN 'integer' THEN 4 WHEN 'bigint' THEN -5 "
             "  WHEN 'real' THEN 6 WHEN 'double' THEN 8 "
             "  WHEN 'boolean' THEN -7 WHEN 'varbinary' THEN -2 "
             "  WHEN 'date' THEN 91 WHEN 'time' THEN 92 WHEN 'timestamp' THEN 93 "
             "  WHEN 'json' THEN -1 WHEN 'array' THEN 2003 WHEN 'map' THEN 2004 "
             "  ELSE 11 END AS SQL_DATA_TYPE, "
             "NULL AS SQL_DATETIME_SUB, "
             "CHARACTER_MAXIMUM_LENGTH AS CHAR_OCTET_LENGTH, "
             "ORDINAL_POSITION, IS_NULLABLE, "
             "NULL AS SCOPE_CATALOG, NULL AS SCOPE_SCHEMA, NULL AS SCOPE_TABLE, "
             "NULL AS DATA_TYPE, NULL AS DATETIME_PRECISION, "
             "NULL AS CHARACTER_SET_CATALOG, NULL AS CHARACTER_SET_SCHEMA, "
             "NULL AS CHARACTER_SET_NAME, NULL AS COLLATION_CATALOG, "
             "NULL AS COLLATION_SCHEMA, NULL AS COLLATION_NAME "
             "FROM %s.information_schema.columns "
             "WHERE TABLE_CATALOG LIKE %s "
             "AND TABLE_SCHEMA LIKE %s "
             "AND TABLE_NAME LIKE %s "
             "AND COLUMN_NAME LIKE %s "
             "ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, ORDINAL_POSITION",
             catalog, cat_pat, schema_pat, table_pat, col_pat);

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLPrimaryKeys
 * ========================================================================
 * Returns: TABLE_CAT, TABLE_SCHEM, TABLE_NAME, COLUMN_NAME, KEY_SEQ, PK_NAME
 * ======================================================================== */

SQLRETURN SQLPrimaryKeys(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                         SQLSMALLINT catalog_name_length, SQLCHAR *schema_name,
                         SQLSMALLINT schema_name_length, SQLCHAR *table_name,
                         SQLSMALLINT table_name_length)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char cat_pat[256], schema_pat[256], table_pat[256];

    build_pattern(catalog_name, catalog_name_length, cat_pat, sizeof(cat_pat));
    build_pattern(schema_name, schema_name_length, schema_pat, sizeof(schema_pat));
    build_pattern(table_name, table_name_length, table_pat, sizeof(table_pat));

    const char *catalog = get_catalog_or_default(stmt->conn);
    snprintf(sql, sizeof(sql),
             "SELECT TABLE_CATALOG AS TABLE_CAT, TABLE_SCHEMA AS TABLE_SCHEM, "
             "TABLE_NAME, COLUMN_NAME, ORDINAL_POSITION AS KEY_SEQ, "
             "CONSTRAINT_NAME AS PK_NAME "
             "FROM %s.information_schema.key_column_usage "
             "WHERE TABLE_CATALOG LIKE %s "
             "AND TABLE_SCHEMA LIKE %s "
             "AND TABLE_NAME LIKE %s "
             "ORDER BY TABLE_CAT, TABLE_SCHEM, TABLE_NAME, KEY_SEQ",
             catalog, cat_pat, schema_pat, table_pat);

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLForeignKeys
 * ========================================================================
 * Returns: PKTABLE_CAT, PKTABLE_SCHEM, PKTABLE_NAME, PKCOLUMN_NAME,
 *          FKTABLE_CAT, FKTABLE_SCHEM, FKTABLE_NAME, FKCOLUMN_NAME,
 *          KEY_SEQ, UPDATE_RULE, DELETE_RULE, FK_NAME, PK_NAME, DEFERRABILITY
 * ======================================================================== */

SQLRETURN SQLForeignKeys(SQLHSTMT statement_handle, SQLCHAR *pk_catalog,
                         SQLSMALLINT pk_catalog_length, SQLCHAR *pk_schema,
                         SQLSMALLINT pk_schema_length, SQLCHAR *pk_table,
                         SQLSMALLINT pk_table_length, SQLCHAR *fk_catalog,
                         SQLSMALLINT fk_catalog_length, SQLCHAR *fk_schema,
                         SQLSMALLINT fk_schema_length, SQLCHAR *fk_table,
                         SQLSMALLINT fk_table_length)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char pk_cat_pat[256], pk_schema_pat[256], pk_table_pat[256];
    char fk_cat_pat[256], fk_schema_pat[256], fk_table_pat[256];

    build_pattern(pk_catalog, pk_catalog_length, pk_cat_pat, sizeof(pk_cat_pat));
    build_pattern(pk_schema, pk_schema_length, pk_schema_pat, sizeof(pk_schema_pat));
    build_pattern(pk_table, pk_table_length, pk_table_pat, sizeof(pk_table_pat));
    build_pattern(fk_catalog, fk_catalog_length, fk_cat_pat, sizeof(fk_cat_pat));
    build_pattern(fk_schema, fk_schema_length, fk_schema_pat, sizeof(fk_schema_pat));
    build_pattern(fk_table, fk_table_length, fk_table_pat, sizeof(fk_table_pat));

    const char *catalog = get_catalog_or_default(stmt->conn);
    snprintf(
        sql, sizeof(sql),
        "SELECT kcu.TABLE_CATALOG AS PKTABLE_CAT, kcu.TABLE_SCHEMA AS PKTABLE_SCHEM, "
        "kcu.TABLE_NAME AS PKTABLE_NAME, kcu.COLUMN_NAME AS PKCOLUMN_NAME, "
        "ccu.TABLE_CATALOG AS FKTABLE_CAT, ccu.TABLE_SCHEMA AS FKTABLE_SCHEM, "
        "ccu.TABLE_NAME AS FKTABLE_NAME, ccu.COLUMN_NAME AS FKCOLUMN_NAME, "
        "kcu.ORDINAL_POSITION AS KEY_SEQ, "
        "rc.UPDATE_RULE, rc.DELETE_RULE, "
        "rc.CONSTRAINT_NAME AS FK_NAME, "
        "tc.CONSTRAINT_NAME AS PK_NAME, "
        "CASE rc.IS_DEFERRABLE WHEN 'YES' THEN 7 ELSE 5 END AS DEFERRABILITY "
        "FROM %s.information_schema.key_column_usage kcu "
        "JOIN %s.information_schema.constraint_column_usage ccu "
        "  ON kcu.CONSTRAINT_NAME = ccu.CONSTRAINT_NAME "
        "  AND kcu.TABLE_SCHEMA = ccu.TABLE_SCHEMA "
        "JOIN %s.information_schema.referential_constraints rc "
        "  ON kcu.CONSTRAINT_NAME = rc.CONSTRAINT_NAME "
        "  AND kcu.TABLE_SCHEMA = rc.CONSTRAINT_SCHEMA "
        "JOIN %s.information_schema.table_constraints tc "
        "  ON rc.UNIQUE_CONSTRAINT_NAME = tc.CONSTRAINT_NAME "
        "  AND tc.TABLE_SCHEMA = kcu.TABLE_SCHEMA "
        "WHERE kcu.TABLE_CATALOG LIKE %s "
        "AND kcu.TABLE_SCHEMA LIKE %s "
        "AND kcu.TABLE_NAME LIKE %s "
        "AND ccu.TABLE_CATALOG LIKE %s "
        "AND ccu.TABLE_SCHEMA LIKE %s "
        "AND ccu.TABLE_NAME LIKE %s "
        "ORDER BY PKTABLE_CAT, PKTABLE_SCHEM, PKTABLE_NAME, KEY_SEQ",
        catalog, catalog, catalog, catalog, pk_cat_pat, pk_schema_pat, pk_table_pat,
        fk_cat_pat, fk_schema_pat, fk_table_pat);

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLGetTablePrivileges
 * ======================================================================== */

SQLRETURN SQLTablePrivileges(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                             SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                             SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                             SQLSMALLINT table_pattern_length)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char cat_pat[256], schema_pat[256], table_pat[256];

    build_pattern(catalog_name, catalog_name_length, cat_pat, sizeof(cat_pat));
    build_pattern(schema_pattern, schema_pattern_length, schema_pat, sizeof(schema_pat));
    build_pattern(table_pattern, table_pattern_length, table_pat, sizeof(table_pat));

    const char *catalog = get_catalog_or_default(stmt->conn);
    snprintf(
        sql, sizeof(sql),
        "SELECT TABLE_CATALOG AS TABLE_CAT, TABLE_SCHEMA AS TABLE_SCHEM, "
        "TABLE_NAME, GRANTEE, 'SELECT' AS PRIVILEGE, 'YES' AS IS_GRANTABLE "
        "FROM %s.information_schema.tables "
        "WHERE TABLE_CATALOG LIKE %s "
        "AND TABLE_SCHEMA LIKE %s "
        "AND TABLE_NAME LIKE %s "
        "UNION ALL "
        "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, GRANTEE, 'INSERT', 'YES' "
        "FROM %s.information_schema.tables "
        "WHERE TABLE_CATALOG LIKE %s AND TABLE_SCHEMA LIKE %s AND TABLE_NAME LIKE %s "
        "UNION ALL "
        "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, GRANTEE, 'UPDATE', 'YES' "
        "FROM %s.information_schema.tables "
        "WHERE TABLE_CATALOG LIKE %s AND TABLE_SCHEMA LIKE %s AND TABLE_NAME LIKE %s "
        "UNION ALL "
        "SELECT TABLE_CATALOG, TABLE_SCHEMA, TABLE_NAME, GRANTEE, 'DELETE', 'YES' "
        "FROM %s.information_schema.tables "
        "WHERE TABLE_CATALOG LIKE %s AND TABLE_SCHEMA LIKE %s AND TABLE_NAME LIKE %s",
        catalog, cat_pat, schema_pat, table_pat, catalog, cat_pat, schema_pat, table_pat,
        catalog, cat_pat, schema_pat, table_pat, catalog, cat_pat, schema_pat, table_pat);

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLGetColumnPrivileges
 * ======================================================================== */

SQLRETURN SQLColumnPrivileges(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                              SQLSMALLINT catalog_name_length, SQLCHAR *schema_pattern,
                              SQLSMALLINT schema_pattern_length, SQLCHAR *table_pattern,
                              SQLSMALLINT table_pattern_length, SQLCHAR *column_pattern,
                              SQLSMALLINT column_pattern_length)
{
    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char cat_pat[256], schema_pat[256], table_pat[256], col_pat[256];

    build_pattern(catalog_name, catalog_name_length, cat_pat, sizeof(cat_pat));
    build_pattern(schema_pattern, schema_pattern_length, schema_pat, sizeof(schema_pat));
    build_pattern(table_pattern, table_pattern_length, table_pat, sizeof(table_pat));
    build_pattern(column_pattern, column_pattern_length, col_pat, sizeof(col_pat));

    const char *catalog = get_catalog_or_default(stmt->conn);
    snprintf(
        sql, sizeof(sql),
        "SELECT TABLE_CATALOG AS TABLE_CAT, TABLE_SCHEMA AS TABLE_SCHEM, "
        "TABLE_NAME, COLUMN_NAME, GRANTEE, 'SELECT' AS PRIVILEGE, 'YES' AS IS_GRANTABLE "
        "FROM %s.information_schema.columns "
        "WHERE TABLE_CATALOG LIKE %s "
        "AND TABLE_SCHEMA LIKE %s "
        "AND TABLE_NAME LIKE %s "
        "AND COLUMN_NAME LIKE %s",
        catalog, cat_pat, schema_pat, table_pat, col_pat);

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLSpecialColumns
 * ======================================================================== */

SQLRETURN SQLSpecialColumns(SQLHSTMT statement_handle, SQLUSMALLINT identifier_type,
                            SQLCHAR *catalog_name, SQLSMALLINT catalog_name_length,
                            SQLCHAR *schema_name, SQLSMALLINT schema_name_length,
                            SQLCHAR *table_name, SQLSMALLINT table_name_length,
                            SQLUSMALLINT identifier_scope, SQLUSMALLINT nullable)
{
    (void)identifier_scope;
    (void)nullable;

    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char cat_pat[256], schema_pat[256], table_pat[256];

    build_pattern(catalog_name, catalog_name_length, cat_pat, sizeof(cat_pat));
    build_pattern(schema_name, schema_name_length, schema_pat, sizeof(schema_pat));
    build_pattern(table_name, table_name_length, table_pat, sizeof(table_pat));

    const char *catalog = get_catalog_or_default(stmt->conn);

    if (identifier_type == SQL_BEST_ROWID || identifier_type == SQL_ROWVER) {
        /* Trino doesn't have native ROWID, but we can return a unique column if available
         */
        snprintf(
            sql, sizeof(sql),
            "SELECT NULL AS SCOPE_CATALOG, NULL AS SCOPE_SCHEMA, "
            "NULL AS SCOPE_TABLE, COLUMN_NAME AS COLUMN_NAME, "
            "DATA_TYPE AS DATA_TYPE, NULL AS TYPE_NAME, "
            "COALESCE(CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, 0) AS COLUMN_SIZE, "
            "BUCKET AS COLUMN_USAGE, NULL AS PSEUDO_COLUMN "
            "FROM %s.information_schema.columns "
            "WHERE TABLE_CATALOG LIKE %s "
            "AND TABLE_SCHEMA LIKE %s "
            "AND TABLE_NAME LIKE %s "
            "AND COLUMN_KEY = 'PRI' "
            "LIMIT 1",
            catalog, cat_pat, schema_pat, table_pat);
    } else {
        /* SQL_UNIQUE */
        snprintf(
            sql, sizeof(sql),
            "SELECT NULL AS SCOPE_CATALOG, NULL AS SCOPE_SCHEMA, "
            "NULL AS SCOPE_TABLE, COLUMN_NAME AS COLUMN_NAME, "
            "DATA_TYPE AS DATA_TYPE, NULL AS TYPE_NAME, "
            "COALESCE(CHARACTER_MAXIMUM_LENGTH, NUMERIC_PRECISION, 0) AS COLUMN_SIZE, "
            "NULL AS COLUMN_USAGE, NULL AS PSEUDO_COLUMN "
            "FROM %s.information_schema.columns "
            "WHERE TABLE_CATALOG LIKE %s "
            "AND TABLE_SCHEMA LIKE %s "
            "AND TABLE_NAME LIKE %s "
            "AND COLUMN_KEY IN ('PRI', 'UNI')",
            catalog, cat_pat, schema_pat, table_pat);
    }

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLStatistics
 * ======================================================================== */

SQLRETURN SQLStatistics(SQLHSTMT statement_handle, SQLCHAR *catalog_name,
                        SQLSMALLINT catalog_name_length, SQLCHAR *schema_name,
                        SQLSMALLINT schema_name_length, SQLCHAR *table_name,
                        SQLSMALLINT table_name_length, SQLUSMALLINT unique,
                        SQLUSMALLINT reserved)
{
    (void)unique;
    (void)reserved;

    if (!statement_handle)
        return SQL_INVALID_HANDLE;

    trino_stmt_t *stmt = (trino_stmt_t *)statement_handle;
    if (!trino_stmt_valid(stmt))
        return SQL_INVALID_HANDLE;
    if (!stmt->conn || !stmt->conn->connected) {
        trino_diag_set_error(&stmt->diagnostics, TRINO_SQLSTATE_INVALID_CONN, 0,
                             "No active connection");
        return SQL_ERROR;
    }

    char sql[TRINO_CATALOG_BUF_SIZE];
    char cat_pat[256], schema_pat[256], table_pat[256];

    build_pattern(catalog_name, catalog_name_length, cat_pat, sizeof(cat_pat));
    build_pattern(schema_name, schema_name_length, schema_pat, sizeof(schema_pat));
    build_pattern(table_name, table_name_length, table_pat, sizeof(table_pat));

    const char *catalog = get_catalog_or_default(stmt->conn);
    snprintf(sql, sizeof(sql),
             "SELECT TABLE_CATALOG AS TABLE_CAT, TABLE_SCHEMA AS TABLE_SCHEM, "
             "TABLE_NAME, "
             "CASE INDEX_TYPE WHEN 'PRIMARY KEY' THEN 0 ELSE 1 END AS NON_UNIQUE, "
             "NULL AS INDEX_QUALIFIER, INDEX_NAME AS INDEX_NAME, "
             "CASE INDEX_TYPE WHEN 'PRIMARY KEY' THEN 1 ELSE 2 END AS TYPE, "
             "ORDINAL_POSITION AS ORDINAL_POSITION, "
             "COLUMN_NAME AS COLUMN_NAME, 'A' AS COLLATION, "
             "0 AS CARDINALITY, 0 AS PAGES, NULL AS FILTER_CONDITION "
             "FROM %s.information_schema.statistics "
             "WHERE TABLE_CATALOG LIKE %s "
             "AND TABLE_SCHEMA LIKE %s "
             "AND TABLE_NAME LIKE %s "
             "ORDER BY NON_UNIQUE, INDEX_NAME, ORDINAL_POSITION",
             catalog, cat_pat, schema_pat, table_pat);

    return SQLExecDirect(statement_handle, (SQLCHAR *)sql, SQL_NTS);
}

/* ========================================================================
 * SQLDataSources / SQLDrivers
 * ======================================================================== */

SQLRETURN SQLDataSources(SQLHENV environment_handle, SQLUSMALLINT direction,
                         SQLCHAR *server_name, SQLSMALLINT buffer_length,
                         SQLSMALLINT *name_length_ptr, SQLCHAR *description,
                         SQLSMALLINT description_buffer_length,
                         SQLSMALLINT *description_length_ptr)
{
    if (!environment_handle)
        return SQL_INVALID_HANDLE;

    trino_env_t *env = (trino_env_t *)environment_handle;
    if (!trino_env_valid(env))
        return SQL_INVALID_HANDLE;

    /* Enumerating data sources is the driver manager's responsibility; a
     * driver normally returns SQL_NO_DATA. Only report on the first fetch. */
    if (direction != SQL_FETCH_FIRST && direction != SQL_FETCH_NEXT) {
        return SQL_NO_DATA;
    }
    if (direction == SQL_FETCH_NEXT) {
        return SQL_NO_DATA;
    }

    if (server_name && buffer_length > 0) {
        strncpy((char *)server_name, "Trino", (size_t)buffer_length - 1);
        ((char *)server_name)[buffer_length - 1] = '\0';
        if (name_length_ptr)
            *name_length_ptr = (SQLSMALLINT)strlen((char *)server_name);
    }

    if (description && description_buffer_length > 0) {
        strncpy((char *)description, "Trino ODBC Driver",
                (size_t)description_buffer_length - 1);
        ((char *)description)[description_buffer_length - 1] = '\0';
        if (description_length_ptr)
            *description_length_ptr = (SQLSMALLINT)strlen((char *)description);
    }

    return SQL_SUCCESS;
}

SQLRETURN SQLDrivers(SQLHENV environment_handle, SQLUSMALLINT driver_completion,
                     SQLCHAR *driver_data, SQLSMALLINT buffer_length,
                     SQLSMALLINT *str_length_ptr, SQLCHAR *driver_attributes,
                     SQLSMALLINT attributes_buffer_length,
                     SQLSMALLINT *attributes_length_ptr)
{
    (void)driver_completion;

    if (!environment_handle)
        return SQL_INVALID_HANDLE;

    trino_env_t *env = (trino_env_t *)environment_handle;
    if (!trino_env_valid(env))
        return SQL_INVALID_HANDLE;

    if (driver_data && buffer_length > 0) {
        strncpy((char *)driver_data, "Trino ODBC Driver", (size_t)buffer_length - 1);
        ((char *)driver_data)[buffer_length - 1] = '\0';
        if (str_length_ptr)
            *str_length_ptr = (SQLSMALLINT)strlen((char *)driver_data);
    }

    if (driver_attributes && attributes_buffer_length > 0) {
        strncpy((char *)driver_attributes,
                "APILevel=2;ConnectFunctions=YYY;DriverODBCVer=03.80",
                (size_t)attributes_buffer_length - 1);
        ((char *)driver_attributes)[attributes_buffer_length - 1] = '\0';
        if (attributes_length_ptr)
            *attributes_length_ptr = (SQLSMALLINT)strlen((char *)driver_attributes);
    }

    return SQL_SUCCESS;
}
