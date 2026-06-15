/* Connection pool placeholder — full connection pooling is deferred.
 * For now, each connection gets its own HTTP client. */

#include <stdlib.h>

/* Stub — connection pooling will be implemented in a later phase. */
void trino_conn_pool_init(void) { /* no-op */ }
void trino_conn_pool_destroy(void) { /* no-op */ }
