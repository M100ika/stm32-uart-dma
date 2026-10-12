#include <pthread.h>
#include <stdint.h>
#include <string.h>

#include "ringbuf.h"
#include "test.h"

static void init_rejects_bad_sizes(void)
{
    uint8_t mem[16];
    ringbuf_t rb;
    CHECK(!rb_init(&rb, mem, 0));
    CHECK(!rb_init(&rb, mem, 1));
    CHECK(!rb_init(&rb, mem, 12));
    CHECK(!rb_init(&rb, NULL, 16));
    CHECK(rb_init(&rb, mem, 16));
    CHECK_EQ(rb_size(&rb), 16);
    CHECK_EQ(rb_count(&rb), 0);
    CHECK_EQ(rb_free(&rb), 16);
}

static void write_then_read(void)
{
    uint8_t mem[8], out[8];
    ringbuf_t rb;
    rb_init(&rb, mem, sizeof(mem));

    CHECK_EQ(rb_write(&rb, "abc", 3), 3);
    CHECK_EQ(rb_count(&rb), 3);
    CHECK_EQ(rb_read(&rb, out, sizeof(out)), 3);
    CHECK(memcmp(out, "abc", 3) == 0);
    CHECK_EQ(rb_count(&rb), 0);
    CHECK_EQ(rb_read(&rb, out, 1), 0);
}

static void partial_write_when_full(void)
{
    uint8_t mem[8], out[8];
    ringbuf_t rb;
    rb_init(&rb, mem, sizeof(mem));

    /* Буфер вмещает ровно size байт: отдельный «пустой» слот не нужен,
     * потому что полный и пустой различаются по head - tail. */
    CHECK_EQ(rb_write(&rb, "0123456789", 10), 8);
    CHECK_EQ(rb_free(&rb), 0);
    CHECK_EQ(rb_write(&rb, "x", 1), 0);
    CHECK_EQ(rb_read(&rb, out, 8), 8);
    CHECK(memcmp(out, "01234567", 8) == 0);
}

static void data_wraps_around_end(void)
{
    uint8_t mem[8], out[8];
    ringbuf_t rb;
    rb_init(&rb, mem, sizeof(mem));

    rb_write(&rb, "123456", 6);
    rb_read(&rb, out, 6);                 /* tail = head = 6 */
    CHECK_EQ(rb_write(&rb, "abcde", 5), 5); /* 2 байта в конец, 3 в начало */
    CHECK_EQ(rb_read(&rb, out, 5), 5);
    CHECK(memcmp(out, "abcde", 5) == 0);
}

static void indices_survive_uint32_overflow(void)
{
    uint8_t mem[8], out[8];
    ringbuf_t rb;
    rb_init(&rb, mem, sizeof(mem));
    rb.head = rb.tail = UINT32_MAX - 2U; /* через 3 байта индексы перейдут через 0 */

    CHECK_EQ(rb_write(&rb, "hello", 5), 5);
    CHECK_EQ(rb_count(&rb), 5);
    CHECK(rb.head < rb.tail);              /* head уже перешёл через ноль */
    CHECK_EQ(rb_free(&rb), 3);
    CHECK_EQ(rb_read(&rb, out, 5), 5);
    CHECK(memcmp(out, "hello", 5) == 0);
    CHECK_EQ(rb_count(&rb), 0);
}

static void peek_contig_stops_at_array_end(void)
{
    uint8_t mem[8];
    ringbuf_t rb;
    rb_init(&rb, mem, sizeof(mem));
    rb.head = rb.tail = 6;
    rb_write(&rb, "ABCDE", 5);

    const uint8_t *p;
    CHECK_EQ(rb_peek_contig(&rb, &p), 2);   /* до конца массива */
    CHECK(p == &mem[6]);
    CHECK(memcmp(p, "AB", 2) == 0);
    rb_skip(&rb, 2);
    CHECK_EQ(rb_peek_contig(&rb, &p), 3);   /* остаток с начала */
    CHECK(p == &mem[0]);
    CHECK(memcmp(p, "CDE", 3) == 0);
    rb_skip(&rb, 100);                      /* больше, чем есть: обрезается */
    CHECK_EQ(rb_count(&rb), 0);
    CHECK_EQ(rb_peek_contig(&rb, &p), 0);
}

/* ---------------------------------------------------------------------
 * Нагрузочный тест: писатель и читатель в разных потоках, как прерывание
 * и основной цикл на МК. Проверяем, что поток байт не теряется и не
 * переставляется. С -fsanitize=thread он же ловит гонки памяти. */

#define STRESS_BYTES 2000000U

static ringbuf_t s_rb;
static uint8_t s_mem[64];

static void *producer(void *arg)
{
    (void)arg;
    uint32_t next = 0;
    while (next < STRESS_BYTES) {
        uint8_t chunk[13];
        uint32_t n = 0;
        while (n < sizeof(chunk) && next + n < STRESS_BYTES) {
            chunk[n] = (uint8_t)((next + n) * 31U);
            n++;
        }
        uint32_t put = rb_write(&s_rb, chunk, n);
        next += put;
    }
    return NULL;
}

static void spsc_threads_keep_order(void)
{
    rb_init(&s_rb, s_mem, sizeof(s_mem));
    pthread_t th;
    pthread_create(&th, NULL, producer, NULL);

    uint32_t got = 0;
    int errors = 0;
    while (got < STRESS_BYTES) {
        uint8_t buf[7];
        uint32_t n = rb_read(&s_rb, buf, sizeof(buf));
        for (uint32_t i = 0; i < n; i++) {
            if (buf[i] != (uint8_t)((got + i) * 31U)) {
                errors++;
            }
        }
        got += n;
    }
    pthread_join(th, NULL);
    CHECK_EQ(got, STRESS_BYTES);
    CHECK_EQ(errors, 0);
}

void run_ringbuf_tests(void)
{
    RUN(init_rejects_bad_sizes);
    RUN(write_then_read);
    RUN(partial_write_when_full);
    RUN(data_wraps_around_end);
    RUN(indices_survive_uint32_overflow);
    RUN(peek_contig_stops_at_array_end);
    RUN(spsc_threads_keep_order);
}
