/* Row buffer management for result sets */

#include "trino_odbc/resultset.h"
#include <stdlib.h>
#include <string.h>

/* Buffer for streaming result rows */
typedef struct {
    char *data;
    size_t capacity;
    size_t used;
} row_buffer_t;

row_buffer_t *row_buffer_create(size_t initial_capacity)
{
    row_buffer_t *buf = calloc(1, sizeof(*buf));
    if (!buf)
        return NULL;

    buf->data = calloc(1, initial_capacity);
    if (!buf->data) {
        free(buf);
        return NULL;
    }
    buf->capacity = initial_capacity;
    buf->used = 0;
    return buf;
}

void row_buffer_destroy(row_buffer_t *buf)
{
    if (!buf)
        return;
    free(buf->data);
    free(buf);
}

static bool row_buffer_ensure(row_buffer_t *buf, size_t needed)
{
    if (buf->used + needed <= buf->capacity)
        return true;

    size_t new_cap = buf->capacity * 2;
    if (new_cap < buf->used + needed)
        new_cap = buf->used + needed;

    char *new_data = realloc(buf->data, new_cap);
    if (!new_data)
        return false;

    buf->data = new_data;
    buf->capacity = new_cap;
    return true;
}

void row_buffer_append(row_buffer_t *buf, const char *src, size_t len)
{
    if (!row_buffer_ensure(buf, len + 1))
        return;
    memcpy(buf->data + buf->used, src, len);
    buf->used += len;
    buf->data[buf->used] = '\0';
}

const char *row_buffer_data(const row_buffer_t *buf)
{
    return buf ? buf->data : NULL;
}

size_t row_buffer_length(const row_buffer_t *buf)
{
    return buf ? buf->used : 0;
}

void row_buffer_reset(row_buffer_t *buf)
{
    if (buf)
        buf->used = 0;
}
