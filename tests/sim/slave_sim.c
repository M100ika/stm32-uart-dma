/*
 * slave_sim.c — симулятор модуля на ПК для проверки протокола
 * с независимой реализацией Modbus-мастера.
 *
 * Использует те же файлы, что и прошивка: modbus.c, mb_rtu.c, app.c.
 * Вместо UART — псевдотерминал (pty): путь к нему печатается в stdout,
 * и к нему подключается мастер (tests/interop_test.py на minimalmodbus).
 * Вместо прерываний и TIM5 — select() и CLOCK_MONOTONIC.
 */
#define _XOPEN_SOURCE 600
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "app.h"
#include "mb_rtu.h"
#include "modbus.h"

#define SLAVE_ADDR 1U
#define BAUD 115200U

static uint32_t now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000U + (uint64_t)ts.tv_nsec / 1000U);
}

int main(void)
{
    int fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (fd < 0 || grantpt(fd) != 0 || unlockpt(fd) != 0) {
        perror("pty");
        return 1;
    }
    printf("%s\n", ptsname(fd));
    fflush(stdout);

    static app_t app;
    app_init(&app);
    app.in.device_id = 0x433;
    app.in.temperature_dc = 245;
    app.in.vdda_mv = 3300;
    app.in.reset_cause = 1;

    mb_rtu_rx_t rx;
    mb_rtu_init(&rx, BAUD);
    mb_counters_t cnt = { 0 };
    uint8_t resp[MB_ADU_MAX];
    uint32_t last_rx = now_us();
    const uint32_t start = now_us();

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        struct timeval tv = { 0, 250 }; /* опрос каждые 250 мкс */
        int r = select(fd + 1, &rfds, NULL, NULL, &tv);
        if (r < 0 && errno != EINTR) {
            perror("select");
            return 1;
        }
        if (r > 0) {
            uint8_t buf[256];
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n < 0 && errno == EIO) {
                /* мастер ещё не открыл или уже закрыл порт */
                usleep(1000);
                continue;
            }
            if (n > 0) {
                mb_rtu_push(&rx, buf, (size_t)n);
                last_rx = now_us();
            }
        }

        size_t len = mb_rtu_poll(&rx, now_us(), last_rx, false);
        if (len > 0U) {
            app.in.uptime_s = (now_us() - start) / 1000000U;
            app.in.mb = cnt;
            size_t out = mb_process(&app.map, SLAVE_ADDR, rx.buf, len, resp, &cnt);
            mb_rtu_reset(&rx);
            if (out > 0U && write(fd, resp, out) != (ssize_t)out) {
                perror("write");
            }
            app_cmd_t cmd = app_take_command(&app);
            if (cmd != APP_CMD_NONE) {
                fprintf(stderr, "command %d\n", (int)cmd); /* на плате здесь был бы сброс */
            }
        }
    }
}
