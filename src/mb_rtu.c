#include "mb_rtu.h"

#include <string.h>

uint32_t mb_rtu_t35_us(uint32_t baud)
{
    if (baud > 19200U) {
        return 1750U; /* фиксированное значение из спецификации Modbus over Serial Line */
    }
    /* 3,5 символа * 11 бит * 1e6 / baud, с округлением вверх. */
    return (uint32_t)((38500000ULL + baud - 1U) / baud);
}

void mb_rtu_init(mb_rtu_rx_t *rx, uint32_t baud)
{
    memset(rx, 0, sizeof(*rx));
    rx->t35_us = mb_rtu_t35_us(baud);
}

void mb_rtu_push(mb_rtu_rx_t *rx, const uint8_t *data, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        if (rx->len < MB_ADU_MAX) {
            rx->buf[rx->len++] = data[i];
        } else {
            rx->overflow = true;
        }
    }
}

size_t mb_rtu_poll(mb_rtu_rx_t *rx, uint32_t now_us, uint32_t last_rx_us, bool pending)
{
    if (rx->len == 0U && !rx->overflow) {
        return 0;
    }
    /* Пока в буфере драйвера есть байты, кадр не закончен. Разность
     * беззнаковых значений корректна и при переполнении счётчика. */
    if (pending || (uint32_t)(now_us - last_rx_us) < rx->t35_us) {
        return 0;
    }
    if (rx->overflow) {
        rx->overflows++;
        mb_rtu_reset(rx); /* слишком длинный кадр — заведомо мусор */
        return 0;
    }
    return rx->len;
}

void mb_rtu_reset(mb_rtu_rx_t *rx)
{
    rx->len = 0;
    rx->overflow = false;
}
