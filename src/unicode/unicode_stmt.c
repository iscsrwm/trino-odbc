/* Unicode (W) variants of the environment, connection, statement, and catalog
 * ODBC entry points.
 *
 * Once the driver exports any W function, the Windows ODBC Driver Manager
 * treats the driver as Unicode and calls the W-suffixed entry points
 * exclusively. .NET's OdbcCommand therefore calls SQLExecDirectW, SQLPrepareW,
 * SQLColAttributeW, SQLSetStmtAttrW, and the catalog W functions. Without these
 * exports the DM fails query execution (often surfaced as a misleading "unable
 * to allocate an environment handle" error).
 *
 * Each wrapper converts its UTF-16 string arguments to UTF-8 and delegates to
 * the existing ANSI implementation. For functions that return character data
 * (SQLColAttributeW), the ANSI result is widened back to UTF-16; per the ODBC
 * convention for the W variants, character buffer lengths and returned string
 * lengths are expressed in bytes. Attribute functions with no string parameters
 * (Set/GetEnvAttr, Set/GetStmtAttr) delegate directly.
 */

#include "trino_odbc/core.h"
#include "trino_odbc/statement.h"
#include "trino_odbc/connection.h"
#include "trino_odbc/protocol.h"
#include "trino_odbc/log.h"
#include <stdlib.h>
#include <string.h>

/* Convert a (SQL_NTS or length-delimited, in characters) SQLWCHAR* to a freshly
 * allocated UTF-8 string. Returns NULL for NULL input. Caller frees. */
static char *w_to_utf8(const SQLWCHAR *w, SQLSMALLINT len)
{
    if (!w)
        return NULL;
    size_t wlen = (len == SQL_NTS) ? trino_wstrlen(w) : (len < 0 ? 0 : (size_t)len);
    if (wlen == 0) {
        char *e = malloc(1);
        if (e)
            e[0] = '\0';
        return e;
    }
    return trino_wchars_to_utf8(w, wlen);
}

/* ========================================================================
 * Environment attributes
 * ======================================================================== */

SQLRETURN SQLSetEnvAttrW(SQLHENV env, SQLINTEGER attr, SQLPOINTER value,
                         SQLINTEGER str_len)
{
    /* Environment attributes are all numeric (passed by value), with no string
     * attributes, so delegate directly to the ANSI version. */
    return SQLSetEnvAttr(env, attr, value, str_len);
}

SQLRETURN SQLGetEnvAttrW(SQLHENV env, SQLINTEGER attr, SQLPOINTER value,
                         SQLINTEGER buffer_length, SQLINTEGER *str_len)
{
    /* Same reasoning as SQLSetEnvAttrW: no string attributes. */
    return SQLGetEnvAttr(env, attr, value, buffer_length, str_len);
}

/* ========================================================================
 * Statement execution
 * ======================================================================== */

SQLRETURN SQLExecDirectW(SQLHSTMT stmt, SQLWCHAR *text, SQLINTEGER text_len)
{
    char *utf8 = w_to_utf8(text, (text_len == SQL_NTS) ? SQL_NTS : (SQLSMALLINT)text_len);
    trino_log("SQLExecDirectW: sql=%s", utf8 ? utf8 : "(null)");
    SQLRETURN ret = SQLExecDirect(stmt, (SQLCHAR *)utf8, utf8 ? SQL_NTS : 0);
    free(utf8);
    return ret;
}

SQLRETURN SQLPrepareW(SQLHSTMT stmt, SQLWCHAR *text, SQLINTEGER text_len)
{
    char *utf8 = w_to_utf8(text, (text_len == SQL_NTS) ? SQL_NTS : (SQLSMALLINT)text_len);
    SQLRETURN ret = SQLPrepare(stmt, (SQLCHAR *)utf8, utf8 ? SQL_NTS : 0);
    free(utf8);
    return ret;
}

/* ========================================================================
 * Statement attributes
 * ======================================================================== */

SQLRETURN SQLSetStmtAttrW(SQLHSTMT stmt, SQLINTEGER attr, SQLPOINTER value,
                          SQLINTEGER str_len)
{
    /* Statement attributes are all numeric (passed by value) or pointer-typed
     * (row status arrays, bind offsets, etc.), with no string attributes, so
     * delegate directly to the ANSI version. */
    trino_log("SQLSetStmtAttrW: entry attr=%d stmt=%p", (int)attr, (void *)stmt);
    SQLRETURN ret = SQLSetStmtAttr(stmt, attr, value, str_len);
    trino_log("SQLSetStmtAttrW: exit ret=%d", ret);
    return ret;
}

SQLRETURN SQLGetStmtAttrW(SQLHSTMT stmt, SQLINTEGER attr, SQLPOINTER value,
                          SQLINTEGER buffer_length, SQLINTEGER *str_len)
{
    /* Same reasoning as SQLSetStmtAttrW: no string attributes. */
    trino_log("SQLGetStmtAttrW: entry attr=%d stmt=%p", (int)attr, (void *)stmt);
    SQLRETURN ret = SQLGetStmtAttr(stmt, attr, value, buffer_length, str_len);
    trino_log("SQLGetStmtAttrW: exit ret=%d", ret);
    return ret;
}

/* ========================================================================
 * Column metadata
 * ======================================================================== */

SQLRETURN SQLColAttributeW(SQLHSTMT stmt, SQLUSMALLINT col, SQLUSMALLINT field,
                           SQLPOINTER char_attr, SQLSMALLINT buffer_length,
                           SQLSMALLINT *string_length, SQLLEN *numeric_attr)
{
    /* Collect the ANSI character attribute into a local buffer, then widen. */
    char ansi[1024] = {0};
    SQLSMALLINT ansi_len = 0;
    SQLRETURN ret =
        SQLColAttribute(stmt, col, field, ansi, (SQLSMALLINT)sizeof(ansi), &ansi_len,
                        numeric_attr);
    if (ret == SQL_ERROR || ret == SQL_INVALID_HANDLE)
        return ret;

    /* Only widen if a character attribute was actually produced. */
    if (char_attr && buffer_length > 0) {
        size_t wlen = 0;
        SQLWCHAR *w = trino_utf8_to_wchars(ansi, &wlen);
        size_t max_wchars = (size_t)buffer_length / sizeof(SQLWCHAR);
        if (max_wchars == 0)
            max_wchars = 1;
        size_t copy = wlen;
        if (copy > max_wchars - 1)
            copy = max_wchars - 1;
        SQLWCHAR *out = (SQLWCHAR *)char_attr;
        if (w && copy > 0)
            memcpy(out, w, copy * sizeof(SQLWCHAR));
        out[copy] = 0;
        if (string_length)
            *string_length = (SQLSMALLINT)(wlen * sizeof(SQLWCHAR));
        free(w);
    } else if (string_length) {
        *string_length = (SQLSMALLINT)(ansi_len * (SQLSMALLINT)sizeof(SQLWCHAR));
    }

    return ret;
}

/* ========================================================================
 * Catalog functions
 * ======================================================================== */

SQLRETURN SQLTablesW(SQLHSTMT stmt, SQLWCHAR *cat, SQLSMALLINT catl, SQLWCHAR *sch,
                     SQLSMALLINT schl, SQLWCHAR *tbl, SQLSMALLINT tbll, SQLWCHAR *typ,
                     SQLSMALLINT typl)
{
    char *c = w_to_utf8(cat, catl), *s = w_to_utf8(sch, schl);
    char *t = w_to_utf8(tbl, tbll), *y = w_to_utf8(typ, typl);
    SQLRETURN ret = SQLTables(stmt, (SQLCHAR *)c, c ? SQL_NTS : 0, (SQLCHAR *)s,
                              s ? SQL_NTS : 0, (SQLCHAR *)t, t ? SQL_NTS : 0,
                              (SQLCHAR *)y, y ? SQL_NTS : 0);
    free(c);
    free(s);
    free(t);
    free(y);
    return ret;
}

SQLRETURN SQLColumnsW(SQLHSTMT stmt, SQLWCHAR *cat, SQLSMALLINT catl, SQLWCHAR *sch,
                      SQLSMALLINT schl, SQLWCHAR *tbl, SQLSMALLINT tbll, SQLWCHAR *col,
                      SQLSMALLINT coll)
{
    char *c = w_to_utf8(cat, catl), *s = w_to_utf8(sch, schl);
    char *t = w_to_utf8(tbl, tbll), *o = w_to_utf8(col, coll);
    SQLRETURN ret = SQLColumns(stmt, (SQLCHAR *)c, c ? SQL_NTS : 0, (SQLCHAR *)s,
                               s ? SQL_NTS : 0, (SQLCHAR *)t, t ? SQL_NTS : 0,
                               (SQLCHAR *)o, o ? SQL_NTS : 0);
    free(c);
    free(s);
    free(t);
    free(o);
    return ret;
}

SQLRETURN SQLStatisticsW(SQLHSTMT stmt, SQLWCHAR *cat, SQLSMALLINT catl, SQLWCHAR *sch,
                         SQLSMALLINT schl, SQLWCHAR *tbl, SQLSMALLINT tbll,
                         SQLUSMALLINT unique, SQLUSMALLINT reserved)
{
    char *c = w_to_utf8(cat, catl), *s = w_to_utf8(sch, schl), *t = w_to_utf8(tbl, tbll);
    SQLRETURN ret = SQLStatistics(stmt, (SQLCHAR *)c, c ? SQL_NTS : 0, (SQLCHAR *)s,
                                  s ? SQL_NTS : 0, (SQLCHAR *)t, t ? SQL_NTS : 0, unique,
                                  reserved);
    free(c);
    free(s);
    free(t);
    return ret;
}

SQLRETURN SQLPrimaryKeysW(SQLHSTMT stmt, SQLWCHAR *cat, SQLSMALLINT catl, SQLWCHAR *sch,
                          SQLSMALLINT schl, SQLWCHAR *tbl, SQLSMALLINT tbll)
{
    char *c = w_to_utf8(cat, catl), *s = w_to_utf8(sch, schl), *t = w_to_utf8(tbl, tbll);
    SQLRETURN ret = SQLPrimaryKeys(stmt, (SQLCHAR *)c, c ? SQL_NTS : 0, (SQLCHAR *)s,
                                   s ? SQL_NTS : 0, (SQLCHAR *)t, t ? SQL_NTS : 0);
    free(c);
    free(s);
    free(t);
    return ret;
}

SQLRETURN SQLSpecialColumnsW(SQLHSTMT stmt, SQLUSMALLINT id_type, SQLWCHAR *cat,
                             SQLSMALLINT catl, SQLWCHAR *sch, SQLSMALLINT schl,
                             SQLWCHAR *tbl, SQLSMALLINT tbll, SQLUSMALLINT scope,
                             SQLUSMALLINT nullable)
{
    char *c = w_to_utf8(cat, catl), *s = w_to_utf8(sch, schl), *t = w_to_utf8(tbl, tbll);
    SQLRETURN ret = SQLSpecialColumns(stmt, id_type, (SQLCHAR *)c, c ? SQL_NTS : 0,
                                      (SQLCHAR *)s, s ? SQL_NTS : 0, (SQLCHAR *)t,
                                      t ? SQL_NTS : 0, scope, nullable);
    free(c);
    free(s);
    free(t);
    return ret;
}
