#include "trino_odbc/core.h"
#include "trino_odbc/connection.h"
#include "trino_odbc/statement.h"
#include "trino_odbc/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ========================================================================
 * Handle pool — maps SQLHANDLE pointers to typed handle structs
 * ======================================================================== */

#define HANDLE_POOL_SIZE 256

typedef struct {
    void *ptr; /* the SQLHANDLE value */
    trino_handle_type_t type;
    bool in_use;
} handle_slot_t;

static handle_slot_t g_pool[HANDLE_POOL_SIZE];
static pthread_mutex_t g_pool_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Find or allocate a slot */
static handle_slot_t *handle_pool_find(void *ptr)
{
    for (size_t i = 0; i < HANDLE_POOL_SIZE; i++) {
        if (g_pool[i].in_use && g_pool[i].ptr == ptr) {
            return &g_pool[i];
        }
    }
    return NULL;
}

static handle_slot_t *handle_pool_alloc(void)
{
    for (size_t i = 0; i < HANDLE_POOL_SIZE; i++) {
        if (!g_pool[i].in_use) {
            g_pool[i].in_use = true;
            return &g_pool[i];
        }
    }
    return NULL; /* pool exhausted */
}

static void handle_pool_free(handle_slot_t *slot)
{
    slot->in_use = false;
    slot->ptr = NULL;
    slot->type = TRINO_HANDLE_ENV;
}

/* ========================================================================
 * SQLAllocHandle
 * ======================================================================== */

SQLRETURN SQLAllocHandle(SQLSMALLINT handle_type, SQLHANDLE input_handle,
                         SQLHANDLE *output_handle)
{
    trino_log("SQLAllocHandle: type=%d input=%p", (int)handle_type,
              (void *)input_handle);

    if (!output_handle) {
        return SQL_INVALID_HANDLE;
    }

    pthread_mutex_lock(&g_pool_mutex);

    switch (handle_type) {
        case SQL_HANDLE_ENV: {
            if (input_handle != SQL_NULL_HANDLE) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            trino_env_t *env = calloc(1, sizeof(trino_env_t));
            if (!env) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }
            env->type = TRINO_HANDLE_ENV;
            env->odbc_version = SQL_OV_ODBC3;
            env->connection_pooling = SQL_CP_OFF;
            env->access_mode = SQL_MODE_READ_WRITE;
            trino_diag_init(&env->diagnostics);
            pthread_mutex_init(&env->mutex, NULL);

            handle_slot_t *slot = handle_pool_alloc();
            if (!slot) {
                /* Pool exhausted: tear down the env we just built. We cannot
                 * report a diagnostic on it since there is no handle to return
                 * to the application. */
                pthread_mutex_destroy(&env->mutex);
                free(env);
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }
            slot->ptr = (void *)env;
            slot->type = TRINO_HANDLE_ENV;
            *output_handle = (SQLHANDLE)env;
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DBC: {
            if (!input_handle) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            trino_env_t *env = (trino_env_t *)input_handle;
            if (!trino_env_valid(env)) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            trino_conn_t *conn = trino_conn_create(env);
            if (!conn) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }

            handle_slot_t *slot = handle_pool_alloc();
            if (!slot) {
                trino_conn_destroy(conn);
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }
            slot->ptr = (void *)conn;
            slot->type = TRINO_HANDLE_DBC;
            *output_handle = (SQLHANDLE)conn;
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_STMT: {
            if (!input_handle) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            trino_conn_t *conn = (trino_conn_t *)input_handle;
            if (!trino_conn_valid(conn)) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            trino_stmt_t *stmt = trino_stmt_create(conn);
            if (!stmt) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }

            handle_slot_t *slot = handle_pool_alloc();
            if (!slot) {
                trino_stmt_destroy(stmt);
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }
            slot->ptr = (void *)stmt;
            slot->type = TRINO_HANDLE_STMT;
            *output_handle = (SQLHANDLE)stmt;
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DESC: {
            if (!input_handle) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            /* For now, we don't support external descriptors fully */
            trino_descriptor_t *desc = trino_desc_create();
            if (!desc) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }

            handle_slot_t *slot = handle_pool_alloc();
            if (!slot) {
                trino_desc_destroy(desc);
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_ERROR;
            }
            slot->ptr = (void *)desc;
            slot->type = TRINO_HANDLE_DESC;
            *output_handle = (SQLHANDLE)desc;
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        default: pthread_mutex_unlock(&g_pool_mutex); return SQL_INVALID_HANDLE;
    }
}

/* ========================================================================
 * SQLFreeHandle
 * ======================================================================== */

SQLRETURN SQLFreeHandle(SQLSMALLINT handle_type, SQLHANDLE handle)
{
    if (!handle) {
        return SQL_INVALID_HANDLE;
    }

    pthread_mutex_lock(&g_pool_mutex);

    handle_slot_t *slot = handle_pool_find(handle);
    if (!slot) {
        pthread_mutex_unlock(&g_pool_mutex);
        return SQL_INVALID_HANDLE;
    }

    switch (handle_type) {
        case SQL_HANDLE_ENV: {
            trino_env_t *env = (trino_env_t *)handle;
            if (!trino_env_valid(env)) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            /* Free all connections in this environment */
            /* (In a full implementation, we'd track connections per env) */
            pthread_mutex_destroy(&env->mutex);
            free(env);
            handle_pool_free(slot);
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DBC: {
            trino_conn_t *conn = (trino_conn_t *)handle;
            if (!trino_conn_valid(conn)) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            trino_conn_destroy(conn);
            handle_pool_free(slot);
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_STMT: {
            trino_stmt_t *stmt = (trino_stmt_t *)handle;
            if (!trino_stmt_valid(stmt)) {
                pthread_mutex_unlock(&g_pool_mutex);
                return SQL_INVALID_HANDLE;
            }
            trino_stmt_destroy(stmt);
            handle_pool_free(slot);
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        case SQL_HANDLE_DESC: {
            trino_descriptor_t *desc = (trino_descriptor_t *)handle;
            trino_desc_destroy(desc);
            handle_pool_free(slot);
            pthread_mutex_unlock(&g_pool_mutex);
            return SQL_SUCCESS;
        }

        default: pthread_mutex_unlock(&g_pool_mutex); return SQL_INVALID_HANDLE;
    }
}

/* ========================================================================
 * Handle validation helpers
 * ======================================================================== */

bool trino_env_valid(trino_env_t *env)
{
    if (!env)
        return false;
    return env->type == TRINO_HANDLE_ENV;
}

bool trino_conn_valid(trino_conn_t *conn)
{
    if (!conn)
        return false;
    return conn->type == TRINO_HANDLE_DBC;
}

bool trino_stmt_valid(trino_stmt_t *stmt)
{
    if (!stmt)
        return false;
    return stmt->type == TRINO_HANDLE_STMT;
}
