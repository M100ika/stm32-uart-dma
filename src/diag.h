/*
 * diag.h — диагностика после сбоев: причина сброса, отчёт о HardFault,
 * независимый сторожевой таймер (IWDG).
 */
#ifndef DIAG_H
#define DIAG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t magic;  /* признак валидной записи */
    uint32_t pc;     /* адрес инструкции, на которой случился сбой */
    uint32_t lr;
    uint32_t psr;
    uint32_t cfsr;   /* причина: UsageFault/BusFault/MemManage */
    uint32_t hfsr;
    uint32_t mmfar;  /* адрес при MemManage, если MMARVALID */
    uint32_t bfar;   /* адрес при BusFault, если BFARVALID */
} crash_report_t;

typedef enum {
    RESET_UNKNOWN = 0,
    RESET_POWER_ON = 1,
    RESET_PIN = 2,
    RESET_SOFTWARE = 3,
    RESET_IWDG = 4,
    RESET_WWDG = 5,
    RESET_LOW_POWER = 6,
    RESET_BROWN_OUT = 7,
} reset_cause_t;

/* Причина последнего сброса по флагам RCC_CSR; флаги после чтения сбрасываются. */
reset_cause_t diag_reset_cause(void);

/* Отчёт о HardFault из прошлого запуска. true — отчёт был (и теперь стёрт). */
bool diag_take_crash(crash_report_t *out);

/* Намеренно вызвать HardFault (для проверки обработчика). */
__attribute__((noreturn)) void diag_trigger_fault(void);

/* Запустить IWDG с таймаутом ~2 с. Остановить его потом нельзя. */
void watchdog_init(void);
void watchdog_feed(void);

#endif /* DIAG_H */
