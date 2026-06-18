/* Trino ODBC driver setup GUI.
 *
 * Implements ConfigDSN / ConfigDSNW, the entry points the Windows ODBC Data
 * Source Administrator (odbcad32.exe) calls to add, configure, or remove a DSN
 * for this driver. The driver's ODBCINST.INI registration sets
 * "Setup"=<path to trino_odbc.dll>, so these live inside the driver DLL itself.
 *
 * The dialog collects the same keywords the connection-string parser
 * understands (see trino_conn_config_t) and persists them to the DSN section of
 * ODBC.INI via SQLWritePrivateProfileString. A "Test Connection" button builds
 * a connection string and dials the server through the driver manager
 * (SQLDriverConnect) so the user gets a real success/failure answer.
 *
 * This file is Windows-only; it is excluded from non-Windows builds.
 */

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sql.h>
#include <sqlext.h>
#include <odbcinst.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "resource.h"
#include "trino_odbc/log.h"

/* The module handle of this DLL, captured in DllMain, needed to load the
 * dialog resource. */
static HINSTANCE g_module = NULL;

/* ODBC.INI keyword names. "DSN" and "Description" are managed by the DM /
 * convention; the rest mirror the connection-string parser keywords. */
#define KW_DESCRIPTION   "Description"
#define KW_SERVER        "Server"
#define KW_PORT          "Port"
#define KW_CATALOG       "Catalog"
#define KW_SCHEMA        "Schema"
#define KW_USER          "User"
#define KW_PASSWORD      "Password"
#define KW_AUTH          "Authentication"
#define KW_SSL           "SSL"
#define KW_SSL_VERIFY    "SSLVerify"
#define KW_SSL_NOREVOKE  "SSLNoRevoke"
#define KW_TRUSTSTORE    "SSLTrustStoreCertificate"
#define KW_SOURCE        "Source"
#define KW_CLIENT_TAGS   "ClientTags"
#define KW_SESSION_PROPS "SessionProperties"
#define KW_QUERY_TIMEOUT "QueryTimeout"
#define KW_CONN_TIMEOUT  "ConnectTimeout"

/* Auth types offered in the combo box (must match what auth.c accepts). */
static const char *const AUTH_TYPES[] = {"NONE", "PASSWORD", "CERTIFICATE",
                                         "KERBEROS"};
#define AUTH_TYPE_COUNT (int)(sizeof(AUTH_TYPES) / sizeof(AUTH_TYPES[0]))

/* In-memory representation of all dialog fields. */
typedef struct {
    char dsn[256];
    char description[256];
    char server[256];
    char port[16];
    char catalog[256];
    char schema[256];
    char user[256];
    char password[512];
    char auth[32];
    int ssl;
    int ssl_verify;
    int ssl_no_revoke;
    char truststore[1024];
    char source[128];
    char client_tags[512];
    char session_props[512];
    char query_timeout[16];
    char conn_timeout[16];

    /* Original DSN name (when reconfiguring an existing DSN, so we can rename). */
    char orig_dsn[256];
    /* True when ConfigDSN was called to add a brand new DSN. */
    BOOL is_new;
} dsn_fields_t;

/* ------------------------------------------------------------------------
 * DllMain - capture the module handle for resource loading.
 * Note: a driver DLL may already have a DllMain elsewhere; this one is for the
 * setup path. If a conflict arises, the handle can instead be captured lazily.
 * ------------------------------------------------------------------------ */
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = hinst;
        DisableThreadLibraryCalls(hinst);
    }
    return TRUE;
}

/* ------------------------------------------------------------------------
 * Registry helpers (ODBC.INI via the installer library).
 * ------------------------------------------------------------------------ */

static void read_kw(const char *dsn, const char *key, const char *dflt, char *out,
                    int out_len)
{
    out[0] = '\0';
    SQLGetPrivateProfileString(dsn, key, dflt ? dflt : "", out, out_len, "ODBC.INI");
    if (out[0] == '\0' && dflt)
        strncpy(out, dflt, (size_t)out_len - 1), out[out_len - 1] = '\0';
}

static BOOL write_kw(const char *dsn, const char *key, const char *value)
{
    return SQLWritePrivateProfileString(dsn, key, value ? value : "", "ODBC.INI");
}

/* Parse common boolean spellings used by the connection-string parser. */
static int parse_bool(const char *s, int dflt)
{
    if (!s || !*s)
        return dflt;
    if (_stricmp(s, "true") == 0 || _stricmp(s, "yes") == 0 || strcmp(s, "1") == 0 ||
        _stricmp(s, "on") == 0)
        return 1;
    if (_stricmp(s, "false") == 0 || _stricmp(s, "no") == 0 || strcmp(s, "0") == 0 ||
        _stricmp(s, "off") == 0)
        return 0;
    return dflt;
}

/* ------------------------------------------------------------------------
 * Load / store all fields.
 * ------------------------------------------------------------------------ */

static void fields_load(dsn_fields_t *f, const char *dsn)
{
    char buf[16];

    memset(f, 0, sizeof(*f));
    if (dsn) {
        strncpy(f->dsn, dsn, sizeof(f->dsn) - 1);
        strncpy(f->orig_dsn, dsn, sizeof(f->orig_dsn) - 1);
    }

    read_kw(dsn, KW_DESCRIPTION, "", f->description, sizeof(f->description));
    read_kw(dsn, KW_SERVER, "localhost", f->server, sizeof(f->server));
    read_kw(dsn, KW_PORT, "8080", f->port, sizeof(f->port));
    read_kw(dsn, KW_CATALOG, "", f->catalog, sizeof(f->catalog));
    read_kw(dsn, KW_SCHEMA, "", f->schema, sizeof(f->schema));
    read_kw(dsn, KW_USER, "", f->user, sizeof(f->user));
    read_kw(dsn, KW_PASSWORD, "", f->password, sizeof(f->password));
    read_kw(dsn, KW_AUTH, "NONE", f->auth, sizeof(f->auth));
    read_kw(dsn, KW_TRUSTSTORE, "", f->truststore, sizeof(f->truststore));
    read_kw(dsn, KW_SOURCE, "trino-odbc", f->source, sizeof(f->source));
    read_kw(dsn, KW_CLIENT_TAGS, "", f->client_tags, sizeof(f->client_tags));
    read_kw(dsn, KW_SESSION_PROPS, "", f->session_props, sizeof(f->session_props));
    read_kw(dsn, KW_QUERY_TIMEOUT, "300", f->query_timeout, sizeof(f->query_timeout));
    read_kw(dsn, KW_CONN_TIMEOUT, "30", f->conn_timeout, sizeof(f->conn_timeout));

    read_kw(dsn, KW_SSL, "false", buf, sizeof(buf));
    f->ssl = parse_bool(buf, 0);
    read_kw(dsn, KW_SSL_VERIFY, "true", buf, sizeof(buf));
    f->ssl_verify = parse_bool(buf, 1);
    read_kw(dsn, KW_SSL_NOREVOKE, "false", buf, sizeof(buf));
    f->ssl_no_revoke = parse_bool(buf, 0);
}

/* Persist the fields to ODBC.INI. Returns FALSE on failure. */
static BOOL fields_save(const dsn_fields_t *f)
{
    /* If the DSN was renamed, remove the old section first. */
    if (f->orig_dsn[0] && _stricmp(f->orig_dsn, f->dsn) != 0) {
        SQLRemoveDSNFromIni(f->orig_dsn);
    }

    /* Create (or refresh) the DSN entry in the DSN list. This associates the DSN
     * name with our driver. */
    if (!SQLWriteDSNToIni(f->dsn, "Trino ODBC Driver")) {
        return FALSE;
    }

    write_kw(f->dsn, KW_DESCRIPTION, f->description);
    write_kw(f->dsn, KW_SERVER, f->server);
    write_kw(f->dsn, KW_PORT, f->port);
    write_kw(f->dsn, KW_CATALOG, f->catalog);
    write_kw(f->dsn, KW_SCHEMA, f->schema);
    write_kw(f->dsn, KW_USER, f->user);
    write_kw(f->dsn, KW_PASSWORD, f->password);
    write_kw(f->dsn, KW_AUTH, f->auth);
    write_kw(f->dsn, KW_SSL, f->ssl ? "true" : "false");
    write_kw(f->dsn, KW_SSL_VERIFY, f->ssl_verify ? "true" : "false");
    write_kw(f->dsn, KW_SSL_NOREVOKE, f->ssl_no_revoke ? "true" : "false");
    write_kw(f->dsn, KW_TRUSTSTORE, f->truststore);
    write_kw(f->dsn, KW_SOURCE, f->source);
    write_kw(f->dsn, KW_CLIENT_TAGS, f->client_tags);
    write_kw(f->dsn, KW_SESSION_PROPS, f->session_props);
    write_kw(f->dsn, KW_QUERY_TIMEOUT, f->query_timeout);
    write_kw(f->dsn, KW_CONN_TIMEOUT, f->conn_timeout);
    return TRUE;
}

/* ------------------------------------------------------------------------
 * Build a DRIVER= connection string from the current fields (for Test).
 * Password is included so the test reflects the real credentials.
 * ------------------------------------------------------------------------ */
static void build_conn_str(const dsn_fields_t *f, char *out, size_t out_len)
{
    int n = _snprintf(out, out_len,
                      "DRIVER={Trino ODBC Driver};Server=%s;Port=%s;Catalog=%s;"
                      "Schema=%s;User=%s;Password=%s;Authentication=%s;SSL=%s;"
                      "SSLVerify=%s;SSLNoRevoke=%s;",
                      f->server, f->port[0] ? f->port : "8080", f->catalog,
                      f->schema, f->user, f->password,
                      f->auth[0] ? f->auth : "NONE", f->ssl ? "true" : "false",
                      f->ssl_verify ? "true" : "false",
                      f->ssl_no_revoke ? "true" : "false");
    if (n < 0 || (size_t)n >= out_len) {
        out[out_len - 1] = '\0';
        return;
    }
    if (f->truststore[0]) {
        _snprintf(out + n, out_len - (size_t)n, "SSLTrustStoreCertificate=%s;",
                  f->truststore);
    }
    out[out_len - 1] = '\0';
}

/* Driver-manager function pointer types (resolved from odbc32.dll at runtime). */
typedef SQLRETURN(SQL_API *PFN_AllocHandle)(SQLSMALLINT, SQLHANDLE, SQLHANDLE *);
typedef SQLRETURN(SQL_API *PFN_FreeHandle)(SQLSMALLINT, SQLHANDLE);
typedef SQLRETURN(SQL_API *PFN_SetEnvAttr)(SQLHENV, SQLINTEGER, SQLPOINTER, SQLINTEGER);
typedef SQLRETURN(SQL_API *PFN_DriverConnect)(SQLHDBC, SQLHWND, SQLCHAR *, SQLSMALLINT,
                                              SQLCHAR *, SQLSMALLINT, SQLSMALLINT *,
                                              SQLUSMALLINT);
typedef SQLRETURN(SQL_API *PFN_Disconnect)(SQLHDBC);
typedef SQLRETURN(SQL_API *PFN_GetDiagRec)(SQLSMALLINT, SQLHANDLE, SQLSMALLINT,
                                           SQLCHAR *, SQLINTEGER *, SQLCHAR *,
                                           SQLSMALLINT, SQLSMALLINT *);

/* Attempt a live connection through the driver manager and report the result.
 *
 * CRITICAL: this code lives inside trino_odbc.dll, which itself EXPORTS the
 * ODBC API (SQLAllocHandle, etc.). If we called those names directly, the
 * linker would bind them to our OWN exported driver functions, creating
 * internal handles the driver manager doesn't recognize - so a later DM call
 * like SQLDriverConnect (which we do NOT export, so it binds to odbc32.dll)
 * receives a foreign handle and returns SQL_ERROR before reaching any driver.
 *
 * To go cleanly through the driver manager, resolve every ODBC entry point from
 * odbc32.dll explicitly via GetProcAddress and call those. */
static void do_test_connection(HWND hdlg, const dsn_fields_t *f)
{
    char conn_str[4096];
    char out_str[1024];
    SQLHENV env = SQL_NULL_HENV;
    SQLHDBC dbc = SQL_NULL_HDBC;
    SQLSMALLINT out_len = 0;
    SQLRETURN ret;
    HMODULE dm;
    PFN_AllocHandle pAllocHandle;
    PFN_FreeHandle pFreeHandle;
    PFN_SetEnvAttr pSetEnvAttr;
    PFN_DriverConnect pDriverConnect;
    PFN_Disconnect pDisconnect;
    PFN_GetDiagRec pGetDiagRec;

    build_conn_str(f, conn_str, sizeof(conn_str));
    {
        /* Log the full connection string with the password redacted. */
        char redacted[4096];
        char *p;
        strncpy(redacted, conn_str, sizeof(redacted) - 1);
        redacted[sizeof(redacted) - 1] = '\0';
        p = redacted;
        while (*p) {
            if (_strnicmp(p, "Password=", 9) == 0) {
                p += 9;
                while (*p && *p != ';')
                    *p++ = '*';
            } else {
                p++;
            }
        }
        trino_log("Test Connection: conn_str=%s", redacted);
    }

    /* Resolve the driver manager entry points explicitly. */
    dm = LoadLibraryA("odbc32.dll");
    if (!dm) {
        MessageBoxA(hdlg, "Could not load the ODBC Driver Manager (odbc32.dll).",
                    "Test Connection", MB_ICONERROR | MB_OK);
        return;
    }
    pAllocHandle = (PFN_AllocHandle)GetProcAddress(dm, "SQLAllocHandle");
    pFreeHandle = (PFN_FreeHandle)GetProcAddress(dm, "SQLFreeHandle");
    pSetEnvAttr = (PFN_SetEnvAttr)GetProcAddress(dm, "SQLSetEnvAttr");
    pDriverConnect = (PFN_DriverConnect)GetProcAddress(dm, "SQLDriverConnect");
    pDisconnect = (PFN_Disconnect)GetProcAddress(dm, "SQLDisconnect");
    pGetDiagRec = (PFN_GetDiagRec)GetProcAddress(dm, "SQLGetDiagRec");

    if (!pAllocHandle || !pFreeHandle || !pSetEnvAttr || !pDriverConnect ||
        !pDisconnect || !pGetDiagRec) {
        FreeLibrary(dm);
        MessageBoxA(hdlg, "Could not resolve ODBC Driver Manager functions.",
                    "Test Connection", MB_ICONERROR | MB_OK);
        return;
    }

    ret = pAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        FreeLibrary(dm);
        MessageBoxA(hdlg, "Failed to allocate ODBC environment.", "Test Connection",
                    MB_ICONERROR | MB_OK);
        return;
    }
    pSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);

    ret = pAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    if (ret != SQL_SUCCESS && ret != SQL_SUCCESS_WITH_INFO) {
        pFreeHandle(SQL_HANDLE_ENV, env);
        FreeLibrary(dm);
        MessageBoxA(hdlg, "Failed to allocate ODBC connection.", "Test Connection",
                    MB_ICONERROR | MB_OK);
        return;
    }

    ret = pDriverConnect(dbc, NULL, (SQLCHAR *)conn_str, SQL_NTS, (SQLCHAR *)out_str,
                         sizeof(out_str), &out_len, SQL_DRIVER_NOPROMPT);
    trino_log("Test Connection: DM SQLDriverConnect ret=%d", (int)ret);

    if (ret == SQL_SUCCESS || ret == SQL_SUCCESS_WITH_INFO) {
        MessageBoxA(hdlg, "Connection successful.", "Test Connection",
                    MB_ICONINFORMATION | MB_OK);
        pDisconnect(dbc);
    } else {
        char msg[2048];
        size_t pos = 0;
        int found = 0;
        SQLSMALLINT rec;
        SQLSMALLINT handle_kinds[2] = {SQL_HANDLE_DBC, SQL_HANDLE_ENV};
        SQLHANDLE handles[2] = {dbc, env};
        int h;

        pos += (size_t)_snprintf(msg + pos, sizeof(msg) - pos,
                                 "Connection failed (rc=%d).\n", (int)ret);

        for (h = 0; h < 2 && pos < sizeof(msg) - 1; h++) {
            for (rec = 1; pos < sizeof(msg) - 1; rec++) {
                SQLCHAR sqlstate[6] = {0};
                SQLCHAR diag[900] = {0};
                SQLINTEGER native = 0;
                SQLSMALLINT diag_len = 0;
                SQLRETURN dr = pGetDiagRec(handle_kinds[h], handles[h], rec, sqlstate,
                                           &native, diag, sizeof(diag), &diag_len);
                if (dr != SQL_SUCCESS && dr != SQL_SUCCESS_WITH_INFO)
                    break;
                found++;
                pos += (size_t)_snprintf(msg + pos, sizeof(msg) - pos,
                                         "\n[%s] (%ld) %s", sqlstate, (long)native,
                                         diag);
            }
        }

        if (!found) {
            _snprintf(msg + pos, sizeof(msg) - pos,
                      "\nNo ODBC diagnostic was returned. Check that the server "
                      "host/port are reachable and the driver is installed "
                      "correctly. Set TRINO_ODBC_LOG to capture a driver trace.");
        }
        msg[sizeof(msg) - 1] = '\0';
        MessageBoxA(hdlg, msg, "Test Connection", MB_ICONERROR | MB_OK);
    }

    pFreeHandle(SQL_HANDLE_DBC, dbc);
    pFreeHandle(SQL_HANDLE_ENV, env);
    FreeLibrary(dm);
}

/* ------------------------------------------------------------------------
 * Dialog <-> fields marshalling.
 * ------------------------------------------------------------------------ */

static void set_text(HWND hdlg, int id, const char *s)
{
    SetDlgItemTextA(hdlg, id, s ? s : "");
}

static void get_text(HWND hdlg, int id, char *out, int out_len)
{
    out[0] = '\0';
    GetDlgItemTextA(hdlg, id, out, out_len);
}

static void dialog_to_fields(HWND hdlg, dsn_fields_t *f)
{
    int sel;
    get_text(hdlg, IDC_DSN_NAME, f->dsn, sizeof(f->dsn));
    get_text(hdlg, IDC_DESCRIPTION, f->description, sizeof(f->description));
    get_text(hdlg, IDC_SERVER, f->server, sizeof(f->server));
    get_text(hdlg, IDC_PORT, f->port, sizeof(f->port));
    get_text(hdlg, IDC_CATALOG, f->catalog, sizeof(f->catalog));
    get_text(hdlg, IDC_SCHEMA, f->schema, sizeof(f->schema));
    get_text(hdlg, IDC_USER, f->user, sizeof(f->user));
    get_text(hdlg, IDC_PASSWORD, f->password, sizeof(f->password));
    get_text(hdlg, IDC_TRUSTSTORE, f->truststore, sizeof(f->truststore));
    get_text(hdlg, IDC_SOURCE, f->source, sizeof(f->source));
    get_text(hdlg, IDC_CLIENT_TAGS, f->client_tags, sizeof(f->client_tags));
    get_text(hdlg, IDC_SESSION_PROPS, f->session_props, sizeof(f->session_props));
    get_text(hdlg, IDC_QUERY_TIMEOUT, f->query_timeout, sizeof(f->query_timeout));
    get_text(hdlg, IDC_CONN_TIMEOUT, f->conn_timeout, sizeof(f->conn_timeout));

    sel = (int)SendDlgItemMessage(hdlg, IDC_AUTH, CB_GETCURSEL, 0, 0);
    if (sel >= 0 && sel < AUTH_TYPE_COUNT) {
        strncpy(f->auth, AUTH_TYPES[sel], sizeof(f->auth) - 1);
        f->auth[sizeof(f->auth) - 1] = '\0';
    }

    f->ssl = (IsDlgButtonChecked(hdlg, IDC_SSL) == BST_CHECKED);
    f->ssl_verify = (IsDlgButtonChecked(hdlg, IDC_SSL_VERIFY) == BST_CHECKED);
    f->ssl_no_revoke = (IsDlgButtonChecked(hdlg, IDC_SSL_NOREVOKE) == BST_CHECKED);
}

static void fields_to_dialog(HWND hdlg, const dsn_fields_t *f)
{
    int i, sel = 0;

    /* Populate the auth combo box. */
    SendDlgItemMessage(hdlg, IDC_AUTH, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < AUTH_TYPE_COUNT; i++) {
        SendDlgItemMessageA(hdlg, IDC_AUTH, CB_ADDSTRING, 0, (LPARAM)AUTH_TYPES[i]);
        if (_stricmp(f->auth, AUTH_TYPES[i]) == 0)
            sel = i;
    }
    SendDlgItemMessage(hdlg, IDC_AUTH, CB_SETCURSEL, (WPARAM)sel, 0);

    set_text(hdlg, IDC_DSN_NAME, f->dsn);
    set_text(hdlg, IDC_DESCRIPTION, f->description);
    set_text(hdlg, IDC_SERVER, f->server);
    set_text(hdlg, IDC_PORT, f->port);
    set_text(hdlg, IDC_CATALOG, f->catalog);
    set_text(hdlg, IDC_SCHEMA, f->schema);
    set_text(hdlg, IDC_USER, f->user);
    set_text(hdlg, IDC_PASSWORD, f->password);
    set_text(hdlg, IDC_TRUSTSTORE, f->truststore);
    set_text(hdlg, IDC_SOURCE, f->source);
    set_text(hdlg, IDC_CLIENT_TAGS, f->client_tags);
    set_text(hdlg, IDC_SESSION_PROPS, f->session_props);
    set_text(hdlg, IDC_QUERY_TIMEOUT, f->query_timeout);
    set_text(hdlg, IDC_CONN_TIMEOUT, f->conn_timeout);

    CheckDlgButton(hdlg, IDC_SSL, f->ssl ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hdlg, IDC_SSL_VERIFY, f->ssl_verify ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(hdlg, IDC_SSL_NOREVOKE,
                   f->ssl_no_revoke ? BST_CHECKED : BST_UNCHECKED);

    /* The DSN name is fixed for an existing DSN (renaming is allowed for new). */
    if (!f->is_new && f->orig_dsn[0]) {
        /* Allow editing but it's unusual; leave enabled for rename support. */
    }
}

/* ------------------------------------------------------------------------
 * Dialog procedure.
 * ------------------------------------------------------------------------ */

static INT_PTR CALLBACK config_dlg_proc(HWND hdlg, UINT msg, WPARAM wparam,
                                        LPARAM lparam)
{
    dsn_fields_t *f;

    switch (msg) {
        case WM_INITDIALOG:
            f = (dsn_fields_t *)lparam;
            SetWindowLongPtr(hdlg, GWLP_USERDATA, (LONG_PTR)f);
            fields_to_dialog(hdlg, f);
            return TRUE;

        case WM_COMMAND:
            f = (dsn_fields_t *)GetWindowLongPtr(hdlg, GWLP_USERDATA);
            switch (LOWORD(wparam)) {
                case IDC_TEST: {
                    dsn_fields_t tmp;
                    memcpy(&tmp, f, sizeof(tmp));
                    dialog_to_fields(hdlg, &tmp);
                    do_test_connection(hdlg, &tmp);
                    return TRUE;
                }
                case IDOK: {
                    dialog_to_fields(hdlg, f);
                    if (f->dsn[0] == '\0') {
                        MessageBoxA(hdlg, "Please enter a Data Source Name.",
                                    "Trino ODBC Driver Setup", MB_ICONWARNING | MB_OK);
                        return TRUE;
                    }
                    EndDialog(hdlg, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hdlg, IDCANCEL);
                    return TRUE;
            }
            break;
    }
    return FALSE;
}

/* ------------------------------------------------------------------------
 * ConfigDSN - the ODBC setup entry point (ANSI).
 *
 * fRequest is one of ODBC_ADD_DSN / ODBC_CONFIG_DSN / ODBC_REMOVE_DSN.
 * lpszAttributes is a double-null-terminated list of key=value pairs the DM
 * already parsed from any DSN= it had (may include a DSN name).
 * ------------------------------------------------------------------------ */

static const char *find_attr(const char *attrs, const char *key, char *out,
                             int out_len)
{
    /* attrs is a list of "key=value\0key=value\0\0". */
    const char *p = attrs;
    size_t klen = strlen(key);
    out[0] = '\0';
    if (!attrs)
        return NULL;
    while (*p) {
        if (_strnicmp(p, key, klen) == 0 && p[klen] == '=') {
            strncpy(out, p + klen + 1, (size_t)out_len - 1);
            out[out_len - 1] = '\0';
            return out;
        }
        p += strlen(p) + 1;
    }
    return NULL;
}

BOOL INSTAPI ConfigDSN(HWND hwndParent, WORD fRequest, LPCSTR lpszDriver,
                       LPCSTR lpszAttributes)
{
    dsn_fields_t f;
    char dsn_name[256] = {0};
    (void)lpszDriver;

    /* Extract the DSN name from the attribute list if present. */
    find_attr(lpszAttributes, "DSN", dsn_name, sizeof(dsn_name));

    switch (fRequest) {
        case ODBC_REMOVE_DSN:
            if (dsn_name[0] == '\0')
                return FALSE;
            return SQLRemoveDSNFromIni(dsn_name);

        case ODBC_ADD_DSN:
        case ODBC_CONFIG_DSN: {
            fields_load(&f, dsn_name[0] ? dsn_name : NULL);
            f.is_new = (fRequest == ODBC_ADD_DSN);

            /* Seed any attributes the DM passed (e.g. SERVER=...) over the loaded
             * defaults so a programmatic SQLConfigDataSource still works. */
            {
                char v[1024];
                if (find_attr(lpszAttributes, KW_SERVER, v, sizeof(v)))
                    strncpy(f.server, v, sizeof(f.server) - 1);
                if (find_attr(lpszAttributes, KW_PORT, v, sizeof(v)))
                    strncpy(f.port, v, sizeof(f.port) - 1);
                if (find_attr(lpszAttributes, KW_CATALOG, v, sizeof(v)))
                    strncpy(f.catalog, v, sizeof(f.catalog) - 1);
                if (find_attr(lpszAttributes, KW_SCHEMA, v, sizeof(v)))
                    strncpy(f.schema, v, sizeof(f.schema) - 1);
                if (find_attr(lpszAttributes, KW_USER, v, sizeof(v)))
                    strncpy(f.user, v, sizeof(f.user) - 1);
            }

            /* If we have a parent window, show the dialog. When hwndParent is
             * NULL the DM is requesting a silent add - just persist. */
            if (hwndParent) {
                INT_PTR rc = DialogBoxParam(g_module, MAKEINTRESOURCE(IDD_CONFIG_DSN),
                                            hwndParent, config_dlg_proc, (LPARAM)&f);
                if (rc != IDOK)
                    return TRUE; /* user cancelled - not an error */
            } else if (dsn_name[0]) {
                strncpy(f.dsn, dsn_name, sizeof(f.dsn) - 1);
            }

            if (f.dsn[0] == '\0')
                return FALSE;

            if (!fields_save(&f)) {
                if (hwndParent)
                    MessageBoxA(hwndParent, "Failed to save the data source.",
                                "Trino ODBC Driver Setup", MB_ICONERROR | MB_OK);
                return FALSE;
            }
            return TRUE;
        }

        default:
            return FALSE;
    }
}

/* ------------------------------------------------------------------------
 * ConfigDSNW - Unicode entry point. The DM may call either; provide both.
 * Convert the wide arguments to ANSI and delegate.
 * ------------------------------------------------------------------------ */
BOOL INSTAPI ConfigDSNW(HWND hwndParent, WORD fRequest, LPCWSTR lpszDriver,
                        LPCWSTR lpszAttributes)
{
    char driver[256] = {0};
    char attrs[4096] = {0};
    size_t i;

    if (lpszDriver)
        WideCharToMultiByte(CP_ACP, 0, lpszDriver, -1, driver, sizeof(driver), NULL,
                            NULL);

    /* Attributes are double-null-terminated; convert the whole block. */
    if (lpszAttributes) {
        const WCHAR *p = lpszAttributes;
        size_t total = 0;
        while (*p) {
            size_t seg = wcslen(p) + 1;
            total += seg;
            p += seg;
        }
        total += 1; /* final terminator */
        /* Convert as a block, preserving embedded NULs. */
        i = 0;
        p = lpszAttributes;
        while (*p && i < sizeof(attrs) - 2) {
            int n = WideCharToMultiByte(CP_ACP, 0, p, -1, attrs + i,
                                        (int)(sizeof(attrs) - i), NULL, NULL);
            if (n <= 0)
                break;
            i += (size_t)n; /* includes the NUL */
            p += wcslen(p) + 1;
        }
        attrs[i] = '\0'; /* extra terminator */
    }

    return ConfigDSN(hwndParent, fRequest, driver[0] ? driver : NULL,
                     attrs[0] ? attrs : NULL);
}

#endif /* _WIN32 */
