/* Control and dialog IDs for the Trino ODBC setup dialog. */
#ifndef TRINO_ODBC_SETUP_RESOURCE_H
#define TRINO_ODBC_SETUP_RESOURCE_H

#define IDD_CONFIG_DSN 101

/* Core fields */
#define IDC_DSN_NAME      1001
#define IDC_DESCRIPTION   1002
#define IDC_SERVER        1003
#define IDC_PORT          1004
#define IDC_CATALOG       1005
#define IDC_SCHEMA        1006
#define IDC_USER          1007
#define IDC_PASSWORD      1008
#define IDC_AUTH          1009  /* combo box */
#define IDC_SSL           1010  /* checkbox */
#define IDC_SSL_VERIFY    1011  /* checkbox */

/* Advanced fields */
#define IDC_SSL_NOREVOKE  1012  /* checkbox */
#define IDC_TRUSTSTORE    1013
#define IDC_SOURCE        1014
#define IDC_CLIENT_TAGS   1015
#define IDC_SESSION_PROPS 1016
#define IDC_QUERY_TIMEOUT 1017
#define IDC_CONN_TIMEOUT  1018

/* Buttons */
#define IDC_TEST          1020

/* Static labels that we don't otherwise reference get IDC_STATIC */
#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

#endif /* TRINO_ODBC_SETUP_RESOURCE_H */
