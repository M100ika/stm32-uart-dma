/*
 * mb_rtu.h — выделение RTU-кадров из потока байт по паузам.
 *
 * В Modbus RTU нет маркеров начала и конца кадра. Кадр заканчивается,
 * когда на линии тишина не меньше 3,5 символов (t3.5). Для скоростей
 * выше 19200 бод спецификация фиксирует t3.5 = 1750 мкс.
 *
 * Время прихода последнего байта фиксирует драйвер UART в прерывании,
 * а основной цикл передаёт его сюда вместе с текущим временем.
 * Модуль не зависит от железа и тестируется на ПК.
 */
#ifndef MB_RTU_H
#define MB_RTU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modbus.h"

typedef struct {
    uint8_t buf[MB_ADU_MAX];
    uint16_t len;
    bool overflow;      /* кадр длиннее 256 байт — будет отброшен целиком */
    uint32_t overflows; /* сколько таких кадров отброшено */
    uint32_t t35_us;
} mb_rtu_rx_t;

/* Пауза t3.5 для скорости baud (1 символ RTU = 11 бит). */
uint32_t mb_rtu_t35_us(uint32_t baud);

void mb_rtu_init(mb_rtu_rx_t *rx, uint32_t baud);

/* Добавить принятые байты к текущему кадру. */
void mb_rtu_push(mb_rtu_rx_t *rx, const uint8_t *data, size_t n);

/*
 * Проверить, закончился ли кадр.
 *   now_us     — текущее время;
 *   last_rx_us — время последнего принятого байта (из прерывания);
 *   pending    — есть ли ещё не переданные сюда байты в буфере драйвера.
 * Возвращает длину готового кадра (кадр в rx->buf) или 0. После обработки
 * кадра вызвать mb_rtu_reset.
 */
size_t mb_rtu_poll(mb_rtu_rx_t *rx, uint32_t now_us, uint32_t last_rx_us, bool pending);

void mb_rtu_reset(mb_rtu_rx_t *rx);

#endif /* MB_RTU_H */
