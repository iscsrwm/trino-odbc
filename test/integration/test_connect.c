/* Integration test stubs — require a running Trino instance */
#include <stdio.h>

int main(void)
{
    printf("Integration tests skipped (no Trino instance configured).\n");
    printf("Set TRINO_INTEGRATION_TEST_URL to enable.\n");
    return 0;
}
