#ifndef TRINO_ODBC_TRANSACTION_H
#define TRINO_ODBC_TRANSACTION_H

#include "trino_odbc/core.h"

/* ========================================================================
 * Transaction management
 * ======================================================================== */

/* Transaction states */
typedef enum {
    TRINO_TXN_NONE,       /* No active transaction */
    TRINO_TXN_ACTIVE,     /* Transaction in progress */
    TRINO_TXN_COMMITTED,  /* Transaction committed */
    TRINO_TXN_ROLLED_BACK /* Transaction rolled back */
} trino_txn_state_t;

/* Transaction isolation levels */
typedef enum {
    TRINO_TXN_READ_UNCOMMITTED = 1,
    TRINO_TXN_READ_COMMITTED   = 2,
    TRINO_TXN_REPEATABLE_READ  = 3,
    TRINO_TXN_SERIALIZABLE     = 4
} trino_txn_isolation_t;

/* Get transaction state */
trino_txn_state_t trino_conn_get_txn_state(trino_conn_t *conn);

/* Set transaction isolation level */
SQLRETURN trino_conn_set_txn_isolation(trino_conn_t *conn, SQLUINTEGER isolation_level);

/* Begin transaction (when autocommit is off) */
SQLRETURN trino_conn_begin_txn(trino_conn_t *conn);

/* Commit transaction */
SQLRETURN SQLCommit(SQLHDBC connection_handle);

/* Rollback transaction */
SQLRETURN SQLRollback(SQLHDBC connection_handle);

/* Set transaction isolation level via ODBC API */
SQLRETURN SQLSetConnectAttr_txn_isolation(SQLHDBC connection_handle, SQLUINTEGER isolation_level);

/* Get transaction isolation level */
SQLRETURN SQLGetConnectAttr_txn_isolation(SQLHDBC connection_handle, SQLUINTEGER *isolation_level);

#endif /* TRINO_ODBC_TRANSACTION_H */
