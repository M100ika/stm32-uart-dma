#include "test.h"

int g_checks;
int g_failures;

int main(void)
{
    printf("ringbuf\n");
    run_ringbuf_tests();
    printf("rx_tracker\n");
    run_rx_tracker_tests();
    printf("modbus\n");
    run_modbus_tests();
    printf("mb_rtu\n");
    run_mb_rtu_tests();
    printf("sensor_calc\n");
    run_sensor_calc_tests();
    printf("app\n");
    run_app_tests();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
