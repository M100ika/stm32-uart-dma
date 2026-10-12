#include "systick.h"

#include "cpu.h"
#include "regs.h"

/* Меняется в прерывании, читается в основном цикле — поэтому volatile.
 * Чтение 32-битного выровненного слова на Cortex-M4 атомарно. */
static volatile uint32_t s_ms;

void systick_init(uint32_t hclk_hz)
{
    SYSTICK->CTRL = 0;
    SYSTICK->LOAD = hclk_hz / 1000U - 1U; /* 24-битный счётчик: до ~16,7 млн */
    SYSTICK->VAL = 0;
    scb_set_exc_priority(SYSTICK_EXC_NUM, 4);
    SYSTICK->CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_TICKINT | SYSTICK_CTRL_ENABLE;
}

uint32_t millis(void)
{
    return s_ms;
}

void SysTick_Handler(void)
{
    s_ms++;
}
