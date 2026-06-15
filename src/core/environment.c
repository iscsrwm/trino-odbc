#include "trino_odbc/core.h"
#include <string.h>

/* ========================================================================
 * SQLSetEnvAttr
 * ======================================================================== */

SQLRETURN SQLSetEnvAttr(SQLHENV environment_handle, SQLINTEGER attribute,
                        SQLPOINTER value_ptr, SQLINTEGER string_length)
{
    (void)string_length; /* most env attrs are numeric */

    if (!environment_handle) {
        return SQL_INVALID_HANDLE;
    }

    trino_env_t *env = (trino_env_t *)environment_handle;
    if (!trino_env_valid(env)) {
        return SQL_INVALID_HANDLE;
    }

    pthread_mutex_lock(&env->mutex);

    switch (attribute) {
        case SQL_ATTR_ODBC_VERSION:
            env->odbc_version = *(SQLUINTEGER *)value_ptr;
            break;

        case SQL_ATTR_CONNECTION_POOLING:
            env->connection_pooling = *(SQLUINTEGER *)value_ptr;
            break;

        case SQL_ATTR_CP_MATCH:
            env->cp_match = *(SQLUINTEGER *)value_ptr;
            break;

        case SQL_ATTR_ACCESS_MODE:
            env->access_mode = *(SQLUINTEGER *)value_ptr;
            break;

        default:
            /* Unknown attribute — silently succeed (ODBC allows this) */
            break;
    }

    pthread_mutex_unlock(&env->mutex);
    return SQL_SUCCESS;
}

/* ========================================================================
 * SQLGetEnvAttr
 * ======================================================================== */

SQLRETURN SQLGetEnvAttr(SQLHENV environment_handle, SQLINTEGER attribute,
                        SQLPOINTER value_ptr, SQLINTEGER buffer_length,
                        SQLINTEGER *string_length_ptr)
{
    if (!environment_handle || !value_ptr) {
        return SQL_INVALID_HANDLE;
    }

    trino_env_t *env = (trino_env_t *)environment_handle;
    if (!trino_env_valid(env)) {
        return SQL_INVALID_HANDLE;
    }

    pthread_mutex_lock(&env->mutex);

    switch (attribute) {
        case SQL_ATTR_ODBC_VERSION:
            if (buffer_length < (SQLINTEGER)sizeof(SQLUINTEGER)) {
                pthread_mutex_unlock(&env->mutex);
                return SQL_ERROR;
            }
            *(SQLUINTEGER *)value_ptr = env->odbc_version;
            if (string_length_ptr) *string_length_ptr = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CONNECTION_POOLING:
            if (buffer_length < (SQLINTEGER)sizeof(SQLUINTEGER)) {
                pthread_mutex_unlock(&env->mutex);
                return SQL_ERROR;
            }
            *(SQLUINTEGER *)value_ptr = env->connection_pooling;
            if (string_length_ptr) *string_length_ptr = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_CP_MATCH:
            if (buffer_length < (SQLINTEGER)sizeof(SQLUINTEGER)) {
                pthread_mutex_unlock(&env->mutex);
                return SQL_ERROR;
            }
            *(SQLUINTEGER *)value_ptr = env->cp_match;
            if (string_length_ptr) *string_length_ptr = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        case SQL_ATTR_ACCESS_MODE:
            if (buffer_length < (SQLINTEGER)sizeof(SQLUINTEGER)) {
                pthread_mutex_unlock(&env->mutex);
                return SQL_ERROR;
            }
            *(SQLUINTEGER *)value_ptr = env->access_mode;
            if (string_length_ptr) *string_length_ptr = (SQLINTEGER)sizeof(SQLUINTEGER);
            break;

        default:
            pthread_mutex_unlock(&env->mutex);
            return SQL_SUCCESS;
    }

    pthread_mutex_unlock(&env->mutex);
    return SQL_SUCCESS;
}
