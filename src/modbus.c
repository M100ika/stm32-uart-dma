#include "modbus.h"

#define FC_READ_COILS          0x01U
#define FC_READ_DISCRETE       0x02U
#define FC_READ_HOLDING        0x03U
#define FC_READ_INPUT          0x04U
#define FC_WRITE_SINGLE_COIL   0x05U
#define FC_WRITE_SINGLE_REG    0x06U
#define FC_WRITE_MULTIPLE_REGS 0x10U

/* Пределы количества из спецификации: ответ должен влезть в 253 байта PDU. */
#define MAX_READ_BITS  2000U
#define MAX_READ_REGS  125U
#define MAX_WRITE_REGS 123U

#define COIL_ON  0xFF00U
#define COIL_OFF 0x0000U

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] << 8 | p[1]);
}

static void wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

/*
 * CRC-16/MODBUS: полином 0x8005 в отражённой форме (0xA001), начальное
 * значение 0xFFFF. Побитовый вариант медленнее табличного, зато не занимает
 * 512 байт флеша; для кадров до 256 байт на 84 МГц это единицы микросекунд.
 */
uint16_t mb_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 1U) ? (uint16_t)((crc >> 1) ^ 0xA001U) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

/* Диапазон [start, start + qty) должен целиком лежать в таблице.
 * Сумма считается в 32 битах, чтобы 0xFFFF + 2 не переполнилось в 1. */
static bool range_ok(uint16_t start, uint16_t qty, uint16_t table_size)
{
    return (uint32_t)start + qty <= table_size;
}

/* Каждый обработчик пишет PDU ответа (начиная с кода функции) в out
 * и возвращает его длину через *out_len, либо код исключения. */

static mb_status_t read_bits(const mb_regmap_t *m, uint8_t fc, const uint8_t *d, size_t dlen,
                             uint8_t *out, size_t *out_len)
{
    const bool coils = (fc == FC_READ_COILS);
    bool (*read)(void *, uint16_t) = coils ? m->read_coil : m->read_discrete;
    const uint16_t table = coils ? m->n_coils : m->n_discrete;
    if (read == NULL) {
        return MB_EX_ILLEGAL_FUNCTION;
    }
    if (dlen != 4U) {
        return MB_EX_ILLEGAL_VALUE;
    }
    const uint16_t start = rd16(d);
    const uint16_t qty = rd16(d + 2);
    if (qty == 0U || qty > MAX_READ_BITS) {
        return MB_EX_ILLEGAL_VALUE;
    }
    if (!range_ok(start, qty, table)) {
        return MB_EX_ILLEGAL_ADDRESS;
    }

    const uint8_t nbytes = (uint8_t)((qty + 7U) / 8U);
    out[0] = fc;
    out[1] = nbytes;
    for (uint8_t i = 0; i < nbytes; i++) {
        out[2 + i] = 0;
    }
    /* Первый бит — младший бит первого байта. */
    for (uint16_t i = 0; i < qty; i++) {
        if (read(m->ctx, (uint16_t)(start + i))) {
            out[2 + i / 8U] |= (uint8_t)(1U << (i % 8U));
        }
    }
    *out_len = 2U + nbytes;
    return MB_OK;
}

static mb_status_t read_regs(const mb_regmap_t *m, uint8_t fc, const uint8_t *d, size_t dlen,
                             uint8_t *out, size_t *out_len)
{
    const bool holding = (fc == FC_READ_HOLDING);
    uint16_t (*read)(void *, uint16_t) = holding ? m->read_holding : m->read_input;
    const uint16_t table = holding ? m->n_holding : m->n_input;
    if (read == NULL) {
        return MB_EX_ILLEGAL_FUNCTION;
    }
    if (dlen != 4U) {
        return MB_EX_ILLEGAL_VALUE;
    }
    const uint16_t start = rd16(d);
    const uint16_t qty = rd16(d + 2);
    if (qty == 0U || qty > MAX_READ_REGS) {
        return MB_EX_ILLEGAL_VALUE;
    }
    if (!range_ok(start, qty, table)) {
        return MB_EX_ILLEGAL_ADDRESS;
    }

    out[0] = fc;
    out[1] = (uint8_t)(qty * 2U);
    for (uint16_t i = 0; i < qty; i++) {
        wr16(&out[2 + 2U * i], read(m->ctx, (uint16_t)(start + i)));
    }
    *out_len = 2U + qty * 2U;
    return MB_OK;
}

static mb_status_t write_coil(const mb_regmap_t *m, const uint8_t *d, size_t dlen,
                              uint8_t *out, size_t *out_len)
{
    if (m->write_coil == NULL) {
        return MB_EX_ILLEGAL_FUNCTION;
    }
    if (dlen != 4U) {
        return MB_EX_ILLEGAL_VALUE;
    }
    const uint16_t addr = rd16(d);
    const uint16_t raw = rd16(d + 2);
    if (raw != COIL_ON && raw != COIL_OFF) {
        return MB_EX_ILLEGAL_VALUE;
    }
    if (addr >= m->n_coils) {
        return MB_EX_ILLEGAL_ADDRESS;
    }
    mb_status_t st = m->write_coil(m->ctx, addr, raw == COIL_ON, false);
    if (st == MB_OK) {
        st = m->write_coil(m->ctx, addr, raw == COIL_ON, true);
    }
    if (st != MB_OK) {
        return st;
    }
    /* Ответ — эхо запроса. */
    out[0] = FC_WRITE_SINGLE_COIL;
    for (int i = 0; i < 4; i++) {
        out[1 + i] = d[i];
    }
    *out_len = 5;
    return MB_OK;
}

static mb_status_t write_reg(const mb_regmap_t *m, const uint8_t *d, size_t dlen,
                             uint8_t *out, size_t *out_len)
{
    if (m->write_holding == NULL) {
        return MB_EX_ILLEGAL_FUNCTION;
    }
    if (dlen != 4U) {
        return MB_EX_ILLEGAL_VALUE;
    }
    const uint16_t addr = rd16(d);
    const uint16_t value = rd16(d + 2);
    if (addr >= m->n_holding) {
        return MB_EX_ILLEGAL_ADDRESS;
    }
    mb_status_t st = m->write_holding(m->ctx, addr, value, false);
    if (st == MB_OK) {
        st = m->write_holding(m->ctx, addr, value, true);
    }
    if (st != MB_OK) {
        return st;
    }
    out[0] = FC_WRITE_SINGLE_REG;
    for (int i = 0; i < 4; i++) {
        out[1 + i] = d[i];
    }
    *out_len = 5;
    return MB_OK;
}

static mb_status_t write_regs(const mb_regmap_t *m, const uint8_t *d, size_t dlen,
                              uint8_t *out, size_t *out_len)
{
    if (m->write_holding == NULL) {
        return MB_EX_ILLEGAL_FUNCTION;
    }
    if (dlen < 5U) {
        return MB_EX_ILLEGAL_VALUE;
    }
    const uint16_t start = rd16(d);
    const uint16_t qty = rd16(d + 2);
    const uint8_t nbytes = d[4];
    if (qty == 0U || qty > MAX_WRITE_REGS || nbytes != qty * 2U || dlen != 5U + nbytes) {
        return MB_EX_ILLEGAL_VALUE;
    }
    if (!range_ok(start, qty, m->n_holding)) {
        return MB_EX_ILLEGAL_ADDRESS;
    }

    /* Проход 1: проверить все значения, ничего не меняя. */
    for (uint16_t i = 0; i < qty; i++) {
        mb_status_t st = m->write_holding(m->ctx, (uint16_t)(start + i), rd16(&d[5 + 2U * i]), false);
        if (st != MB_OK) {
            return st;
        }
    }
    /* Проход 2: применить. Ошибка здесь означала бы, что проверка неполна. */
    for (uint16_t i = 0; i < qty; i++) {
        mb_status_t st = m->write_holding(m->ctx, (uint16_t)(start + i), rd16(&d[5 + 2U * i]), true);
        if (st != MB_OK) {
            return MB_EX_DEVICE_FAILURE;
        }
    }

    out[0] = FC_WRITE_MULTIPLE_REGS;
    wr16(&out[1], start);
    wr16(&out[3], qty);
    *out_len = 5;
    return MB_OK;
}

static bool is_write(uint8_t fc)
{
    return fc == FC_WRITE_SINGLE_COIL || fc == FC_WRITE_SINGLE_REG || fc == FC_WRITE_MULTIPLE_REGS;
}

size_t mb_process(const mb_regmap_t *map, uint8_t my_addr,
                  const uint8_t *req, size_t req_len,
                  uint8_t *resp, mb_counters_t *cnt)
{
    /* Минимальный кадр: адрес + функция + CRC. */
    if (req_len < 4U || req_len > MB_ADU_MAX) {
        cnt->bad_frames++;
        return 0;
    }
    /* CRC передаётся младшим байтом вперёд — в отличие от данных. */
    const uint16_t crc_rx = (uint16_t)(req[req_len - 2U] | (uint16_t)req[req_len - 1U] << 8);
    if (mb_crc16(req, req_len - 2U) != crc_rx) {
        cnt->crc_errors++;
        return 0; /* на испорченный кадр не отвечают: адрес мог быть чужим */
    }

    const uint8_t addr = req[0];
    const bool broadcast = (addr == MB_BROADCAST_ADDR);
    if (!broadcast && addr != my_addr) {
        cnt->not_for_us++;
        return 0;
    }

    const uint8_t fc = req[1];
    const uint8_t *d = &req[2];
    const size_t dlen = req_len - 4U;

    /* Широковещательно имеют смысл только команды записи. */
    if (broadcast && !is_write(fc)) {
        cnt->not_for_us++;
        return 0;
    }

    uint8_t *pdu = &resp[1];
    size_t pdu_len = 0;
    mb_status_t st;
    switch (fc) {
    case FC_READ_COILS:
    case FC_READ_DISCRETE:
        st = read_bits(map, fc, d, dlen, pdu, &pdu_len);
        break;
    case FC_READ_HOLDING:
    case FC_READ_INPUT:
        st = read_regs(map, fc, d, dlen, pdu, &pdu_len);
        break;
    case FC_WRITE_SINGLE_COIL:
        st = write_coil(map, d, dlen, pdu, &pdu_len);
        break;
    case FC_WRITE_SINGLE_REG:
        st = write_reg(map, d, dlen, pdu, &pdu_len);
        break;
    case FC_WRITE_MULTIPLE_REGS:
        st = write_regs(map, d, dlen, pdu, &pdu_len);
        break;
    default:
        st = MB_EX_ILLEGAL_FUNCTION;
        break;
    }

    cnt->frames_ok++;
    if (broadcast) {
        return 0;
    }

    if (st != MB_OK) {
        pdu[0] = (uint8_t)(fc | 0x80U);
        pdu[1] = (uint8_t)st;
        pdu_len = 2;
        cnt->exceptions++;
    }

    resp[0] = my_addr;
    const size_t n = 1U + pdu_len;
    const uint16_t crc = mb_crc16(resp, n);
    resp[n] = (uint8_t)crc;
    resp[n + 1U] = (uint8_t)(crc >> 8);
    return n + 2U;
}
