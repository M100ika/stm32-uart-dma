#include "rx_tracker.h"
#include "test.h"

static void no_new_data(void)
{
    rx_tracker_t t;
    rx_span_t s[2];
    rx_tracker_init(&t, 64);
    CHECK_EQ(rx_tracker_update(&t, 0, s), 0);
}

static void linear_chunks(void)
{
    rx_tracker_t t;
    rx_span_t s[2];
    rx_tracker_init(&t, 64);

    CHECK_EQ(rx_tracker_update(&t, 5, s), 1);   /* IDLE после 5 байт */
    CHECK_EQ(s[0].offset, 0);
    CHECK_EQ(s[0].len, 5);

    CHECK_EQ(rx_tracker_update(&t, 32, s), 1);  /* HT */
    CHECK_EQ(s[0].offset, 5);
    CHECK_EQ(s[0].len, 27);
}

static void tc_at_buffer_end(void)
{
    rx_tracker_t t;
    rx_span_t s[2];
    rx_tracker_init(&t, 64);
    rx_tracker_update(&t, 32, s);

    /* TC: NDTR уже перезагружен в 64, позиция 0 — данные до конца буфера. */
    CHECK_EQ(rx_tracker_update(&t, 0, s), 1);
    CHECK_EQ(s[0].offset, 32);
    CHECK_EQ(s[0].len, 32);
}

static void pos_equal_to_size_means_zero(void)
{
    rx_tracker_t t;
    rx_span_t s[2];
    rx_tracker_init(&t, 64);
    rx_tracker_update(&t, 40, s);

    /* NDTR прочитан как 0 в момент перезагрузки: pos = 64 -> 0. */
    CHECK_EQ(rx_tracker_update(&t, 64, s), 1);
    CHECK_EQ(s[0].offset, 40);
    CHECK_EQ(s[0].len, 24);
    CHECK_EQ(t.last_pos, 0);
}

static void wrap_gives_two_spans(void)
{
    rx_tracker_t t;
    rx_span_t s[2];
    rx_tracker_init(&t, 64);
    rx_tracker_update(&t, 60, s);

    CHECK_EQ(rx_tracker_update(&t, 4, s), 2);
    CHECK_EQ(s[0].offset, 60);
    CHECK_EQ(s[0].len, 4);
    CHECK_EQ(s[1].offset, 0);
    CHECK_EQ(s[1].len, 4);
}

static void total_bytes_match_stream(void)
{
    /* Имитируем приход 1000 байт пачками разной длины и проверяем,
     * что сумма отрезков равна числу пришедших байт. */
    rx_tracker_t t;
    rx_span_t s[2];
    rx_tracker_init(&t, 64);
    unsigned pos = 0, total = 0, sent = 0;
    const unsigned sizes[] = { 1, 7, 31, 32, 13, 2, 63 };
    for (unsigned i = 0; sent < 1000; i++) {
        unsigned n = sizes[i % 7];
        sent += n;
        pos = (pos + n) % 64;
        unsigned k = rx_tracker_update(&t, (uint16_t)pos, s);
        for (unsigned j = 0; j < k; j++) {
            total += s[j].len;
        }
    }
    CHECK_EQ(total, sent);
}

void run_rx_tracker_tests(void)
{
    RUN(no_new_data);
    RUN(linear_chunks);
    RUN(tc_at_buffer_end);
    RUN(pos_equal_to_size_means_zero);
    RUN(wrap_gives_two_spans);
    RUN(total_bytes_match_stream);
}
