/*
 * clock.h — тактирование: HSI 16 МГц -> PLL -> SYSCLK 84 МГц.
 *
 * 84 МГц — максимум для F401 и допустимая частота для F446, поэтому одна
 * конфигурация подходит обеим платам Nucleo. Внешний кварц не нужен:
 * на Nucleo HSE берётся с ST-LINK и зависит от перемычек.
 *
 *   HSI 16 МГц / M(16) = 1 МГц  (вход PLL, допустимо 1..2 МГц)
 *   1 МГц * N(336)     = 336 МГц (VCO, допустимо 100..432 МГц)
 *   VCO / P(4)         = 84 МГц  -> SYSCLK, HCLK
 *   VCO / Q(7)         = 48 МГц  -> USB OTG FS / SDIO (здесь не используются)
 *   APB1 = HCLK / 2    = 42 МГц  (предел APB1 у F401 — 42 МГц)
 *   APB2 = HCLK / 1    = 84 МГц
 */
#ifndef CLOCK_H
#define CLOCK_H

#include <stdbool.h>
#include <stdint.h>

#define HSI_HZ 16000000UL

/* Возвращает false, если PLL не запустилась; тогда система остаётся на HSI. */
bool clock_init(void);

uint32_t clock_hclk_hz(void);
uint32_t clock_pclk1_hz(void);

/* Частота таймеров на APB1 (TIM2..TIM5). Если делитель APB1 не равен 1,
 * таймеры тактируются удвоенной частотой шины: 42 МГц * 2 = 84 МГц. */
uint32_t clock_apb1_timer_hz(void);

#endif /* CLOCK_H */
