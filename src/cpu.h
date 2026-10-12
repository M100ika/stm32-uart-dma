/*
 * cpu.h — инструкции ядра Cortex-M4 и работа с NVIC.
 */
#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include "regs.h"

static inline void cpu_dsb(void) { __asm volatile("dsb 0xF" ::: "memory"); }
static inline void cpu_isb(void) { __asm volatile("isb 0xF" ::: "memory"); }
static inline void cpu_wfi(void) { __asm volatile("wfi"); }

/*
 * Критическая секция: запоминаем PRIMASK и запрещаем прерывания.
 * Восстанавливаем прежнее значение, а не просто разрешаем: так секции
 * можно вкладывать, и функция не включит прерывания, если её вызвали
 * из кода, который их уже выключил.
 */
static inline uint32_t irq_save(void)
{
    uint32_t primask;
    __asm volatile("mrs %0, primask\n\tcpsid i" : "=r"(primask) :: "memory");
    return primask;
}

static inline void irq_restore(uint32_t primask)
{
    __asm volatile("msr primask, %0" :: "r"(primask) : "memory");
}

static inline void nvic_enable(unsigned irq)
{
    NVIC_ISER[irq >> 5] = 1UL << (irq & 31U);
}

/* prio: 0 (наивысший) .. 15. В регистре используются старшие 4 бита. */
static inline void nvic_set_priority(unsigned irq, unsigned prio)
{
    NVIC_IPR[irq] = (uint8_t)(prio << (8U - NVIC_PRIO_BITS));
}

static inline void scb_set_exc_priority(unsigned exc_num, unsigned prio)
{
    SCB->SHPR[exc_num - 4U] = (uint8_t)(prio << (8U - NVIC_PRIO_BITS));
}

__attribute__((noreturn)) static inline void cpu_reset(void)
{
    cpu_dsb();
    SCB->AIRCR = SCB_AIRCR_SYSRESET;
    cpu_dsb();
    for (;;) {
    }
}

#endif /* CPU_H */
