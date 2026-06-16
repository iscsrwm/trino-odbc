#include "trino_odbc/core.h"
#include <stdlib.h>
#include <string.h>

trino_descriptor_t *trino_desc_create(void)
{
    trino_descriptor_t *desc = calloc(1, sizeof(*desc));
    if (!desc)
        return NULL;

    desc->records = calloc(DEFAULT_DESCRIPTOR_RECORDS, sizeof(trino_desc_record_t));
    if (!desc->records) {
        free(desc);
        return NULL;
    }

    desc->record_count = DEFAULT_DESCRIPTOR_RECORDS;
    desc->alloc_count = DEFAULT_DESCRIPTOR_RECORDS;
    desc->bind_type = 0; /* SQL_BIND_BY_COLUMN */
    desc->bind_offsets = NULL;
    return desc;
}

void trino_desc_destroy(trino_descriptor_t *desc)
{
    if (!desc)
        return;
    free(desc->records);
    free(desc->bind_offsets);
    free(desc);
}

static trino_desc_record_t *trino_desc_get_record(trino_descriptor_t *desc,
                                                  SQLUSMALLINT rec)
{
    if (rec == 0 || rec > (SQLUSMALLINT)desc->record_count) {
        return NULL;
    }
    return &desc->records[rec - 1];
}

SQLRETURN trino_desc_set_field(trino_descriptor_t *desc, SQLUSMALLINT rec_num,
                               SQLINTEGER field, SQLPOINTER value, SQLINTEGER str_len)
{
    (void)str_len;
    if (!desc)
        return SQL_ERROR;

    /* Handle count field */
    if (field == SQL_DESC_COUNT) {
        SQLULEN new_count = *(SQLULEN *)value;
        if (new_count > desc->alloc_count) {
            /* Grow the descriptor */
            SQLULEN new_alloc = new_count * 2;
            trino_desc_record_t *new_records =
                realloc(desc->records, new_alloc * sizeof(trino_desc_record_t));
            if (!new_records)
                return SQL_ERROR;
            memset(&new_records[desc->alloc_count], 0,
                   (new_alloc - desc->alloc_count) * sizeof(trino_desc_record_t));
            desc->records = new_records;
            desc->alloc_count = new_alloc;
        }
        desc->record_count = new_count;
        return SQL_SUCCESS;
    }

    if (rec_num == 0)
        return SQL_ERROR;

    trino_desc_record_t *rec = trino_desc_get_record(desc, rec_num);
    if (!rec)
        return SQL_ERROR;

    switch (field) {
        case SQL_DESC_TYPE: rec->sql_type = *(SQLSMALLINT *)value; break;
        case SQL_DESC_LENGTH: rec->column_size = *(SQLULEN *)value; break;
        case SQL_DESC_PRECISION: rec->decimal_digits = *(SQLSMALLINT *)value; break;
        case SQL_DESC_SCALE: rec->decimal_digits = *(SQLSMALLINT *)value; break;
        case SQL_DESC_NULLABLE: rec->nullable = *(SQLSMALLINT *)value; break;
        case SQL_DESC_DATA_PTR: rec->data_ptr = value; break;
        case SQL_DESC_OCTET_LENGTH: rec->buffer_length = *(SQLLEN *)value; break;
        case SQL_DESC_INDICATOR_PTR: rec->str_len_or_ind = (SQLLEN *)value; break;
        default: return SQL_SUCCESS; /* silently ignore unknown fields */
    }
    return SQL_SUCCESS;
}

SQLRETURN trino_desc_get_field(trino_descriptor_t *desc, SQLUSMALLINT rec_num,
                               SQLINTEGER field, SQLPOINTER value,
                               SQLINTEGER buffer_length, SQLINTEGER *str_len)
{
    if (!desc)
        return SQL_ERROR;

    /* Handle count field */
    if (field == SQL_DESC_COUNT) {
        *(SQLULEN *)value = desc->record_count;
        if (str_len)
            *str_len = (SQLINTEGER)sizeof(SQLULEN);
        return SQL_SUCCESS;
    }

    if (rec_num == 0)
        return SQL_ERROR;

    trino_desc_record_t *rec = trino_desc_get_record(desc, rec_num);
    if (!rec)
        return SQL_ERROR;

    switch (field) {
        case SQL_DESC_TYPE:
            *(SQLSMALLINT *)value = rec->sql_type;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLSMALLINT);
            break;
        case SQL_DESC_LENGTH:
            *(SQLULEN *)value = rec->column_size;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLULEN);
            break;
        case SQL_DESC_OCTET_LENGTH:
            *(SQLLEN *)value = rec->buffer_length;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLLEN);
            break;
        case SQL_DESC_PRECISION:
            *(SQLULEN *)value = rec->column_size;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLULEN);
            break;
        case SQL_DESC_SCALE:
            *(SQLSMALLINT *)value = rec->decimal_digits;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLSMALLINT);
            break;
        case SQL_DESC_NULLABLE:
            *(SQLSMALLINT *)value = rec->nullable;
            if (str_len)
                *str_len = (SQLINTEGER)sizeof(SQLSMALLINT);
            break;
        case SQL_DESC_NAME:
            if (value && buffer_length > 0) {
                strncpy((char *)value, (char *)rec->column_name,
                        (size_t)buffer_length - 1);
                ((char *)value)[buffer_length - 1] = '\0';
                if (str_len)
                    *str_len = (SQLINTEGER)strlen((char *)value);
            }
            break;
        case SQL_DESC_TYPE_NAME:
            if (value && buffer_length > 0) {
                strncpy((char *)value, (char *)rec->type_name, (size_t)buffer_length - 1);
                ((char *)value)[buffer_length - 1] = '\0';
                if (str_len)
                    *str_len = (SQLINTEGER)strlen((char *)value);
            }
            break;
        default: return SQL_SUCCESS;
    }
    return SQL_SUCCESS;
}
