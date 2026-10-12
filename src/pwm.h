/*
 * pwm.h — ШИМ на TIM2 CH1 (PA5, светодиод LD2 на Nucleo).
 * Частота 1 кГц, яркость задаётся в промилле: 0..1000.
 * На реальном модуле этот же выход управлял бы, например, клапаном
 * или вентилятором через транзисторный ключ.
 */
#ifndef PWM_H
#define PWM_H

#include <stdint.h>

#define PWM_MAX 1000U

void pwm_init(uint32_t tim_clk_hz);
void pwm_set(uint16_t permille);

#endif /* PWM_H */
