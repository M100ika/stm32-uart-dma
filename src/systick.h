/*
 * systick.h — системный таймер с шагом 1 мс.
 */
#ifndef SYSTICK_H
#define SYSTICK_H

#include <stdbool.h>
#include <stdint.h>

void systick_init(uint32_t hclk_hz);

/* Миллисекунды с запуска. Переполняется через ~49,7 суток. */
uint32_t millis(void);

/*
 * Истёк ли интервал. Разность беззнаковых чисел корректна и при
 * переполнении счётчика: (5 - 0xFFFFFFFB) == 10. Поэтому время всегда
 * сравниваем через разность, а не как now >= start + ms.
 */
static inline bool elapsed(uint32_t now, uint32_t start, uint32_t ms)
{
    return (uint32_t)(now - start) >= ms;
}

#endif /* SYSTICK_H */
