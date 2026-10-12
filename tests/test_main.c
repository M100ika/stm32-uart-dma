#include "test.h"

int g_checks;
int g_failures;

int main(void)
{
    printf("ringbuf\n");
    run_ringbuf_tests();
    printf("rx_tracker\n");
    run_rx_tracker_tests();
    printf("cli\n");
    run_cli_tests();
    printf("strutil\n");
    run_strutil_tests();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
