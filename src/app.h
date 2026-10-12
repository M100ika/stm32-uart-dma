/*
 * app.h — карта регистров Modbus модуля ввода-вывода.
 *
 * Модуль не обращается к железу: основной цикл перед обработкой каждого
 * кадра кладёт свежие показания в app_t.in, а после — забирает выходы
 * (яркость ШИМ, отложенную команду). Поэтому карта регистров проверяется
 * на ПК и используется в симуляторе для теста с независимым мастером.
 *
 * Адреса — смещения от нуля внутри таблицы (в документации к SCADA их
 * часто пишут как 00001, 10001, 30001, 40001 соответственно).
 */
#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#include "modbus.h"

#define FW_VERSION_REG 0x0200U   /* 2.0 */

/* Coils (чтение/запись битов) */
enum {
    COIL_OUTPUT_ON = 0,     /* 1 — выход на 100 %, 0 — выключен */
    COIL_BLINK = 1,         /* 1 — режим мигания */
    COIL_COUNT
};

/* Discrete inputs (только чтение битов) */
enum {
    DI_BUTTON = 0,          /* кнопка нажата сейчас */
    DI_COUNT
};

/* Input registers (только чтение) */
enum {
    IR_DEVICE_ID = 0,       /* код кристалла: 0x0433 F401, 0x0421 F446 */
    IR_FW_VERSION,
    IR_UPTIME_HI,           /* время работы, с — старшие 16 бит */
    IR_UPTIME_LO,           /*                   младшие 16 бит */
    IR_BUTTON_PRESSES,
    IR_TEMPERATURE,         /* температура кристалла, 0,1 °C, со знаком */
    IR_VDDA_MV,             /* напряжение питания АЦП, мВ */
    IR_RESET_CAUSE,         /* reset_cause_t */
    IR_CRASH_PC_HI,         /* адрес HardFault прошлого запуска, 0 — сбоя не было */
    IR_CRASH_PC_LO,
    IR_FRAMES_OK,           /* счётчики по модулю 65536 */
    IR_CRC_ERRORS,
    IR_EXCEPTIONS,
    IR_UART_ERRORS,
    IR_RX_DROPPED,
    IR_COUNT
};

/* Holding registers (чтение/запись) */
enum {
    HR_OUTPUT_LEVEL = 0,    /* уровень ШИМ-выхода, 0..1000 ‰ */
    HR_OUTPUT_MODE,         /* 0 — постоянно, 1 — мигание */
    HR_BLINK_PERIOD_MS,     /* полупериод мигания, 50..10000 мс */
    HR_COMMAND,             /* запись выполняет команду, чтение всегда 0 */
    HR_SCRATCH0,            /* 8 регистров для данных мастера */
    HR_COUNT = HR_SCRATCH0 + 8
};

/* Значения для HR_COMMAND: «магические» коды защищают от случайной записи. */
#define CMD_CODE_RESET 0xA5A5U
#define CMD_CODE_FAULT 0xDEADU   /* проверка обработчика HardFault */
#define CMD_CODE_HANG  0x5741U   /* проверка сторожевого таймера */

typedef enum { APP_CMD_NONE, APP_CMD_RESET, APP_CMD_FAULT, APP_CMD_HANG } app_cmd_t;

/* Показания, которые основной цикл снимает перед обработкой кадра.
 * Снимок делается один раз на кадр, поэтому 32-битные значения
 * (время работы, адрес сбоя) читаются согласованно — без ситуации,
 * когда старшая половина взята до перехода секунды, а младшая после. */
typedef struct {
    uint16_t device_id;
    uint32_t uptime_s;
    uint32_t button_presses;
    bool button_pressed;
    int16_t temperature_dc;
    uint16_t vdda_mv;
    uint16_t reset_cause;
    uint32_t crash_pc;
    mb_counters_t mb;
    uint32_t uart_errors;
    uint32_t rx_dropped;
} app_inputs_t;

typedef struct {
    app_inputs_t in;
    uint16_t output_level;
    uint16_t output_mode;
    uint16_t blink_period_ms;
    uint16_t scratch[8];
    app_cmd_t pending;
    mb_regmap_t map;
} app_t;

void app_init(app_t *app);

/* Уровень ШИМ-выхода с учётом режима мигания. */
uint16_t app_output_level(const app_t *app, uint32_t now_ms);

/* Команда, записанная в HR_COMMAND; выполнять после отправки ответа. */
app_cmd_t app_take_command(app_t *app);

#endif /* APP_H */
