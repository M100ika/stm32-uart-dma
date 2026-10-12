#include <stdint.h>

#include "mb_rtu.h"
#include "test.h"

static void t35_by_baud(void)
{
    CHECK_EQ(mb_rtu_t35_us(9600), 4011);    /* 3,5 * 11 / 9600 = 4010,4 мкс, вверх */
    CHECK_EQ(mb_rtu_t35_us(19200), 2006);
    CHECK_EQ(mb_rtu_t35_us(38400), 1750);   /* выше 19200 — фиксировано */
    CHECK_EQ(mb_rtu_t35_us(115200), 1750);
}

static void frame_ends_after_silence(void)
{
    mb_rtu_rx_t rx;
    mb_rtu_init(&rx, 115200);
    const uint8_t part1[] = { 1, 3, 0 };
    const uint8_t part2[] = { 0, 0, 1, 0x84, 0x0A };
    mb_rtu_push(&rx, part1, 3);
    CHECK_EQ(mb_rtu_poll(&rx, 1000, 1000, false), 0);   /* только что пришёл байт */
    mb_rtu_push(&rx, part2, 5);
    CHECK_EQ(mb_rtu_poll(&rx, 2749, 1000, false), 0);   /* 1749 мкс — ещё рано */
    CHECK_EQ(mb_rtu_poll(&rx, 2750, 1000, false), 8);   /* ровно t3.5 */
    CHECK_EQ(rx.buf[7], 0x0A);
    mb_rtu_reset(&rx);
    CHECK_EQ(mb_rtu_poll(&rx, 9000, 1000, false), 0);   /* после сброса пусто */
}

static void pending_bytes_delay_frame_end(void)
{
    mb_rtu_rx_t rx;
    mb_rtu_init(&rx, 115200);
    const uint8_t b[] = { 1, 2 };
    mb_rtu_push(&rx, b, 2);
    /* Время вышло, но в буфере драйвера ещё лежат байты — ждём. */
    CHECK_EQ(mb_rtu_poll(&rx, 10000, 0, true), 0);
    CHECK_EQ(mb_rtu_poll(&rx, 10000, 0, false), 2);
}

static void timer_wraparound(void)
{
    mb_rtu_rx_t rx;
    mb_rtu_init(&rx, 115200);
    const uint8_t b[] = { 1, 2, 3, 4 };
    mb_rtu_push(&rx, b, 4);
    const uint32_t last = UINT32_MAX - 500U;
    CHECK_EQ(mb_rtu_poll(&rx, last + 1000U, last, false), 0);  /* now перешёл через 0 */
    CHECK_EQ(mb_rtu_poll(&rx, last + 1750U, last, false), 4);
}

static void oversized_frame_is_dropped(void)
{
    mb_rtu_rx_t rx;
    mb_rtu_init(&rx, 115200);
    uint8_t junk[100] = { 0 };
    for (int i = 0; i < 3; i++) {
        mb_rtu_push(&rx, junk, sizeof(junk));   /* 300 байт > 256 */
    }
    CHECK(rx.overflow);
    CHECK_EQ(mb_rtu_poll(&rx, 5000, 0, false), 0);
    CHECK_EQ(rx.len, 0);
    CHECK(!rx.overflow);
    CHECK_EQ(rx.overflows, 1);
}

void run_mb_rtu_tests(void)
{
    RUN(t35_by_baud);
    RUN(frame_ends_after_silence);
    RUN(pending_bytes_delay_frame_end);
    RUN(timer_wraparound);
    RUN(oversized_frame_is_dropped);
}
