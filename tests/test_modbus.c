#include <stdarg.h>
#include <string.h>

#include "modbus.h"
#include "test.h"

/* ------------------------------------------------ тестовая карта регистров */

static bool s_coils[10];
static bool s_discrete[10];
static uint16_t s_input[10];
static uint16_t s_holding[10];

#define HR_LIMITED 5          /* принимает только значения 0..1000 */
#define COIL_BROKEN 9         /* запись в него «ломает устройство» */

static bool rd_coil(void *ctx, uint16_t a) { (void)ctx; return s_coils[a]; }
static bool rd_disc(void *ctx, uint16_t a) { (void)ctx; return s_discrete[a]; }
static uint16_t rd_in(void *ctx, uint16_t a) { (void)ctx; return s_input[a]; }
static uint16_t rd_hr(void *ctx, uint16_t a) { (void)ctx; return s_holding[a]; }

static mb_status_t wr_coil(void *ctx, uint16_t a, bool v, bool commit)
{
    (void)ctx;
    if (a == COIL_BROKEN) {
        return MB_EX_DEVICE_FAILURE;
    }
    if (commit) {
        s_coils[a] = v;
    }
    return MB_OK;
}

static mb_status_t wr_hr(void *ctx, uint16_t a, uint16_t v, bool commit)
{
    (void)ctx;
    if (a == HR_LIMITED && v > 1000U) {
        return MB_EX_ILLEGAL_VALUE;
    }
    if (commit) {
        s_holding[a] = v;
    }
    return MB_OK;
}

static const mb_regmap_t MAP = {
    .n_coils = 10, .n_discrete = 10, .n_input = 10, .n_holding = 10,
    .read_coil = rd_coil, .write_coil = wr_coil, .read_discrete = rd_disc,
    .read_input = rd_in, .read_holding = rd_hr, .write_holding = wr_hr,
};

#define ME 0x11

static mb_counters_t s_cnt;
static uint8_t s_resp[MB_ADU_MAX];
static size_t s_resp_len;

static void reset(void)
{
    memset(s_coils, 0, sizeof(s_coils));
    memset(s_discrete, 0, sizeof(s_discrete));
    for (int i = 0; i < 10; i++) {
        s_input[i] = (uint16_t)(0x100 + i);
        s_holding[i] = (uint16_t)(0x200 + i);
    }
    memset(&s_cnt, 0, sizeof(s_cnt));
}

/* Собрать кадр из байт (без CRC), дописать CRC и обработать. */
static void send(int n, ...)
{
    uint8_t req[MB_ADU_MAX];
    va_list ap;
    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        req[i] = (uint8_t)va_arg(ap, int);
    }
    va_end(ap);
    uint16_t crc = mb_crc16(req, (size_t)n);
    req[n] = (uint8_t)crc;
    req[n + 1] = (uint8_t)(crc >> 8);
    s_resp_len = mb_process(&MAP, ME, req, (size_t)n + 2U, s_resp, &s_cnt);
}

/* Проверить, что ответ совпадает с ожидаемыми байтами и CRC верна. */
static int resp_is(int n, ...)
{
    if (s_resp_len != (size_t)n + 2U) {
        printf("    response length %zu, expected %d\n", s_resp_len, n + 2);
        return 0;
    }
    va_list ap;
    va_start(ap, n);
    int ok = 1;
    for (int i = 0; i < n; i++) {
        if (s_resp[i] != (uint8_t)va_arg(ap, int)) {
            printf("    byte %d: 0x%02X\n", i, s_resp[i]);
            ok = 0;
        }
    }
    va_end(ap);
    uint16_t crc = mb_crc16(s_resp, (size_t)n);
    return ok && s_resp[n] == (uint8_t)crc && s_resp[n + 1] == (uint8_t)(crc >> 8);
}

/* ------------------------------------------------------------------ тесты */

static void crc_known_vectors(void)
{
    /* Контрольное значение CRC-16/MODBUS для ASCII "123456789". */
    CHECK_EQ(mb_crc16((const uint8_t *)"123456789", 9), 0x4B37);
    /* Пример из спецификации: 01 03 00 00 00 0A -> CRC C5 CD на линии. */
    const uint8_t req[] = { 0x01, 0x03, 0x00, 0x00, 0x00, 0x0A };
    CHECK_EQ(mb_crc16(req, sizeof(req)), 0xCDC5);
}

static void read_holding(void)
{
    reset();
    send(6, ME, 0x03, 0x00, 0x01, 0x00, 0x02);
    CHECK(resp_is(7, ME, 0x03, 4, 0x02, 0x01, 0x02, 0x02));
    CHECK_EQ(s_cnt.frames_ok, 1);
}

static void read_input(void)
{
    reset();
    send(6, ME, 0x04, 0x00, 0x09, 0x00, 0x01);
    CHECK(resp_is(5, ME, 0x04, 2, 0x01, 0x09));
}

static void read_coils_packs_bits_lsb_first(void)
{
    reset();
    s_coils[0] = s_coils[2] = s_coils[3] = s_coils[8] = true;
    send(6, ME, 0x01, 0x00, 0x00, 0x00, 0x09);
    CHECK(resp_is(5, ME, 0x01, 2, 0x0D, 0x01));
}

static void read_discrete(void)
{
    reset();
    s_discrete[1] = true;
    send(6, ME, 0x02, 0x00, 0x01, 0x00, 0x01);
    CHECK(resp_is(4, ME, 0x02, 1, 0x01));
}

static void write_single_coil(void)
{
    reset();
    send(6, ME, 0x05, 0x00, 0x03, 0xFF, 0x00);
    CHECK(resp_is(6, ME, 0x05, 0x00, 0x03, 0xFF, 0x00)); /* эхо */
    CHECK(s_coils[3]);
    send(6, ME, 0x05, 0x00, 0x03, 0x00, 0x00);
    CHECK(!s_coils[3]);
}

static void write_coil_rejects_bad_value(void)
{
    reset();
    send(6, ME, 0x05, 0x00, 0x03, 0x12, 0x34);   /* только FF00 или 0000 */
    CHECK(resp_is(3, ME, 0x85, 0x03));
    send(6, ME, 0x05, 0x00, 0x0A, 0xFF, 0x00);   /* адрес вне таблицы */
    CHECK(resp_is(3, ME, 0x85, 0x02));
    send(6, ME, 0x05, 0x00, COIL_BROKEN, 0xFF, 0x00);
    CHECK(resp_is(3, ME, 0x85, 0x04));
    CHECK_EQ(s_cnt.exceptions, 3);
}

static void write_single_register(void)
{
    reset();
    send(6, ME, 0x06, 0x00, HR_LIMITED, 0x03, 0xE8);  /* 1000 */
    CHECK(resp_is(6, ME, 0x06, 0x00, HR_LIMITED, 0x03, 0xE8));
    CHECK_EQ(s_holding[HR_LIMITED], 1000);
    send(6, ME, 0x06, 0x00, HR_LIMITED, 0x03, 0xE9);  /* 1001 — нельзя */
    CHECK(resp_is(3, ME, 0x86, 0x03));
    CHECK_EQ(s_holding[HR_LIMITED], 1000);
}

static void write_multiple_registers(void)
{
    reset();
    send(11, ME, 0x10, 0x00, 0x01, 0x00, 0x02, 4, 0xAA, 0xBB, 0xCC, 0xDD);
    CHECK(resp_is(6, ME, 0x10, 0x00, 0x01, 0x00, 0x02));
    CHECK_EQ(s_holding[1], 0xAABB);
    CHECK_EQ(s_holding[2], 0xCCDD);
}

static void write_multiple_is_all_or_nothing(void)
{
    reset();
    /* Регистры 4, 5, 6: значение для 5 недопустимо -> не меняется ни один. */
    send(13, ME, 0x10, 0x00, 0x04, 0x00, 0x03, 6, 0x00, 0x01, 0xFF, 0xFF, 0x00, 0x02);
    CHECK(resp_is(3, ME, 0x90, 0x03));
    CHECK_EQ(s_holding[4], 0x204);
    CHECK_EQ(s_holding[6], 0x206);
}

static void write_multiple_checks_byte_count(void)
{
    reset();
    send(11, ME, 0x10, 0x00, 0x01, 0x00, 0x02, 3, 0xAA, 0xBB, 0xCC, 0xDD);
    CHECK(resp_is(3, ME, 0x90, 0x03));
}

static void quantity_and_range_checks(void)
{
    reset();
    send(6, ME, 0x03, 0x00, 0x00, 0x00, 0x00);   /* количество 0 */
    CHECK(resp_is(3, ME, 0x83, 0x03));
    send(6, ME, 0x03, 0x00, 0x00, 0x00, 126);    /* больше 125 */
    CHECK(resp_is(3, ME, 0x83, 0x03));
    send(6, ME, 0x03, 0x00, 0x09, 0x00, 0x02);   /* выходит за таблицу */
    CHECK(resp_is(3, ME, 0x83, 0x02));
    send(6, ME, 0x03, 0xFF, 0xFF, 0x00, 0x02);   /* 0xFFFF + 2 не должно переполниться */
    CHECK(resp_is(3, ME, 0x83, 0x02));
}

static void unknown_function(void)
{
    reset();
    send(2, ME, 0x2B);
    CHECK(resp_is(3, ME, 0xAB, 0x01));
}

static void bad_crc_is_ignored(void)
{
    reset();
    uint8_t req[] = { ME, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00 };
    CHECK_EQ(mb_process(&MAP, ME, req, sizeof(req), s_resp, &s_cnt), 0);
    CHECK_EQ(s_cnt.crc_errors, 1);
}

static void other_address_is_ignored(void)
{
    reset();
    send(6, ME + 1, 0x03, 0x00, 0x00, 0x00, 0x01);
    CHECK_EQ(s_resp_len, 0);
    CHECK_EQ(s_cnt.not_for_us, 1);
}

static void broadcast_write_applies_silently(void)
{
    reset();
    send(6, 0x00, 0x06, 0x00, 0x02, 0x12, 0x34);
    CHECK_EQ(s_resp_len, 0);
    CHECK_EQ(s_holding[2], 0x1234);
    send(6, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01); /* чтение широковещательно не имеет смысла */
    CHECK_EQ(s_resp_len, 0);
}

static void short_frame_is_rejected(void)
{
    reset();
    uint8_t req[] = { ME, 0x03, 0x00 };
    CHECK_EQ(mb_process(&MAP, ME, req, sizeof(req), s_resp, &s_cnt), 0);
    CHECK_EQ(s_cnt.bad_frames, 1);
}

static uint16_t rd_addr(void *ctx, uint16_t a) { (void)ctx; return (uint16_t)(0x1000U + a); }

static void read_max_registers_fits_buffer(void)
{
    /* 125 регистров — самый длинный ответ: 3 + 250 + 2 = 255 байт. */
    reset();
    mb_regmap_t big = MAP;
    big.n_input = 300;
    big.read_input = rd_addr;
    uint8_t req[8] = { ME, 0x04, 0x00, 0x00, 0x00, 125 };
    uint16_t crc = mb_crc16(req, 6);
    req[6] = (uint8_t)crc;
    req[7] = (uint8_t)(crc >> 8);
    CHECK_EQ(mb_process(&big, ME, req, 8, s_resp, &s_cnt), 255);
    CHECK_EQ(s_resp[2], 250);
    CHECK_EQ(s_resp[3 + 2 * 124], 0x10);       /* регистр 124 = 0x107C */
    CHECK_EQ(s_resp[3 + 2 * 124 + 1], 0x7C);
}

static void missing_table_is_illegal_function(void)
{
    reset();
    mb_regmap_t no_coils = MAP;
    no_coils.read_coil = NULL;
    uint8_t req[8] = { ME, 0x01, 0x00, 0x00, 0x00, 0x01 };
    uint16_t crc = mb_crc16(req, 6);
    req[6] = (uint8_t)crc;
    req[7] = (uint8_t)(crc >> 8);
    CHECK_EQ(mb_process(&no_coils, ME, req, 8, s_resp, &s_cnt), 5);
    CHECK_EQ(s_resp[1], 0x81);
    CHECK_EQ(s_resp[2], 0x01);
}

void run_modbus_tests(void)
{
    RUN(crc_known_vectors);
    RUN(read_holding);
    RUN(read_input);
    RUN(read_coils_packs_bits_lsb_first);
    RUN(read_discrete);
    RUN(write_single_coil);
    RUN(write_coil_rejects_bad_value);
    RUN(write_single_register);
    RUN(write_multiple_registers);
    RUN(write_multiple_is_all_or_nothing);
    RUN(write_multiple_checks_byte_count);
    RUN(quantity_and_range_checks);
    RUN(unknown_function);
    RUN(bad_crc_is_ignored);
    RUN(other_address_is_ignored);
    RUN(broadcast_write_applies_silently);
    RUN(short_frame_is_rejected);
    RUN(read_max_registers_fits_buffer);
    RUN(missing_table_is_illegal_function);
}
