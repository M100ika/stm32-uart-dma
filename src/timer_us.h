/*
 * timer_us.h — микросекундное время на 32-битном TIM5.
 *
 * SysTick даёт шаг 1 мс, а паузу между кадрами Modbus (1750 мкс) нужно
 * мерить точнее. TIM5 считает с частотой 1 МГц и переполняется через ~71 мин;
 * сравнения времени делаются через беззнаковую разность.
 */
#ifndef TIMER_US_H
#define TIMER_US_H

#include <stdint.h>

void timer_us_init(uint32_t tim_clk_hz);
uint32_t timer_us_now(void);

#endif /* TIMER_US_H */
