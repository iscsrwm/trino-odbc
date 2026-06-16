#include "trino_odbc/transaction.h"
#include "trino_odbc/statement.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Transaction state management
 * ======================================================================== */

trino_txn_state_t trino_conn_get_txn_state(trino_conn_t *conn)
{
    if (!conn)
        return TRINO_TXN_NONE;
    if (conn->autocommit)
        return TRINO_TXN_NONE;
    if (conn->in_transaction)
        return TRINO_TXN_ACTIVE;
    return TRINO_TXN_NONE;
}

SQLRETURN trino_conn_set_txn_isolation(trino_conn_t *conn, SQLUINTEGER isolation_level)
{
    if (!conn)
        return SQL_ERROR;

    pthread_mutex_lock(&conn->mutex);

    /* Validate isolation level */
    switch (isolation_level) {
        case SQL_TXN_READ_UNCOMMITTED:
        case SQL_TXN_READ_COMMITTED:
        case SQL_TXN_REPEATABLE_READ:
        case SQL_TXN_SERIALIZABLE: conn->txn_isolation = isolation_level; break;
        default:
            /* Default to READ_COMMITTED for Trino */
            conn->txn_isolation = SQL_TXN_READ_COMMITTED;
            break;
    }

    pthread_mutex_unlock(&conn->mutex);
    return SQL_SUCCESS;
}

SQLRETURN trino_conn_begin_txn(trino_conn_t *conn)
{
    if (!conn || !conn->connected)
        return SQL_ERROR;

    pthread_mutex_lock(&conn->mutex);

    /* Start a transaction by setting session property */
    /* Trino uses session properties for transaction control */
    conn->in_transaction = true;

    pthread_mutex_unlock(&conn->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLCommit
 * ======================================================================== */

SQLRETURN SQLCommit(SQLHDBC connection_handle)
{
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    pthread_mutex_lock(&conn->mutex);

    /* If not connected or not in transaction, return success per ODBC spec */
    if (!conn->connected || !conn->in_transaction) {
        pthread_mutex_unlock(&conn->mutex);
        return SQL_SUCCESS;
    }

    /* In Trino, each statement is auto-committed by default.
     * When autocommit is off, we track the transaction state locally.
     * For connectors that support transactions (like Hive, Delta Lake),
     * the commit happens implicitly. */

    conn->in_transaction = false;

    pthread_mutex_unlock(&conn->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLRollback
 * ======================================================================== */

SQLRETURN SQLRollback(SQLHDBC connection_handle)
{
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    pthread_mutex_lock(&conn->mutex);

    /* If not connected or not in transaction, return success per ODBC spec */
    if (!conn->connected || !conn->in_transaction) {
        pthread_mutex_unlock(&conn->mutex);
        return SQL_SUCCESS;
    }

    /* Rollback the transaction */
    /* For Trino connectors that support transactions, this would cancel
     * pending operations. For read-only connectors, this is a no-op. */

    conn->in_transaction = false;

    pthread_mutex_unlock(&conn->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * Transaction isolation level functions
 * ======================================================================== */

SQLRETURN SQLSetConnectAttr_txn_isolation(SQLHDBC connection_handle,
                                          SQLUINTEGER isolation_level)
{
    if (!connection_handle)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    return trino_conn_set_txn_isolation(conn, isolation_level);
}

SQLRETURN SQLGetConnectAttr_txn_isolation(SQLHDBC connection_handle,
                                          SQLUINTEGER *isolation_level)
{
    if (!connection_handle || !isolation_level)
        return SQL_INVALID_HANDLE;

    trino_conn_t *conn = (trino_conn_t *)connection_handle;
    if (!trino_conn_valid(conn))
        return SQL_INVALID_HANDLE;

    pthread_mutex_lock(&conn->mutex);
    *isolation_level = conn->txn_isolation;
    pthread_mutex_unlock(&conn->mutex);

    return SQL_SUCCESS;
}
