/*
 * modbus.h — ядро Modbus RTU slave: проверка кадра, разбор функций, ответ.
 *
 * Модуль не знает ни про UART, ни про конкретные регистры. Приложение
 * описывает карту регистров набором функций (mb_regmap_t), а ядро
 * проверяет адреса, количества и CRC и собирает ответ или исключение.
 * Поэтому протокол целиком тестируется на ПК.
 *
 * Поддерживаемые функции:
 *   0x01 Read Coils             0x05 Write Single Coil
 *   0x02 Read Discrete Inputs   0x06 Write Single Register
 *   0x03 Read Holding Registers 0x10 Write Multiple Registers
 *   0x04 Read Input Registers
 *
 * Формат RTU-кадра: адрес (1) | функция (1) | данные (0..252) | CRC16 (2, младший байт первым).
 * Числа внутри данных передаются старшим байтом вперёд (big-endian).
 */
#ifndef MODBUS_H
#define MODBUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MB_ADU_MAX 256U          /* максимальная длина RTU-кадра */
#define MB_BROADCAST_ADDR 0U     /* запись всем устройствам, без ответа */

typedef enum {
    MB_OK = 0,
    MB_EX_ILLEGAL_FUNCTION = 0x01,
    MB_EX_ILLEGAL_ADDRESS = 0x02,
    MB_EX_ILLEGAL_VALUE = 0x03,
    MB_EX_DEVICE_FAILURE = 0x04,
} mb_status_t;

/*
 * Карта регистров. Адреса — смещения от нуля внутри каждой таблицы
 * (так их передаёт протокол; «40001» — это нотация документации, а не адрес).
 * Ядро вызывает функции только для адресов меньше n_*.
 *
 * Запись проходит в два прохода: сначала commit = false для всех значений
 * (только проверка), затем commit = true. Так запрос на запись нескольких
 * регистров либо применяется целиком, либо не меняет ничего.
 */
typedef struct {
    uint16_t n_coils;
    uint16_t n_discrete;
    uint16_t n_input;
    uint16_t n_holding;
    bool (*read_coil)(void *ctx, uint16_t addr);
    mb_status_t (*write_coil)(void *ctx, uint16_t addr, bool value, bool commit);
    bool (*read_discrete)(void *ctx, uint16_t addr);
    uint16_t (*read_input)(void *ctx, uint16_t addr);
    uint16_t (*read_holding)(void *ctx, uint16_t addr);
    mb_status_t (*write_holding)(void *ctx, uint16_t addr, uint16_t value, bool commit);
    void *ctx;
} mb_regmap_t;

typedef struct {
    uint32_t frames_ok;    /* обработано кадров, адресованных нам */
    uint32_t crc_errors;   /* CRC не сошлась — кадр отброшен молча */
    uint32_t bad_frames;   /* короче минимальной длины */
    uint32_t not_for_us;   /* другой адрес */
    uint32_t exceptions;   /* отправлено ответов-исключений */
} mb_counters_t;

uint16_t mb_crc16(const uint8_t *data, size_t len);

/*
 * Обработать принятый кадр. resp — буфер на MB_ADU_MAX байт.
 * Возвращает длину ответа; 0 — отвечать не нужно (чужой адрес,
 * широковещательный запрос, испорченный кадр).
 */
size_t mb_process(const mb_regmap_t *map, uint8_t my_addr,
                  const uint8_t *req, size_t req_len,
                  uint8_t *resp, mb_counters_t *cnt);

#endif /* MODBUS_H */
