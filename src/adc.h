/*
 * adc.h — встроенные датчики через ADC1: температура кристалла (канал 18)
 * и внутреннее опорное напряжение VREFINT (канал 17).
 * Внешних компонентов не нужно — удобно для демонстрации на голой плате.
 */
#ifndef ADC_H
#define ADC_H

#include <stdbool.h>
#include <stdint.h>

void adc_init(void);

/* Одно преобразование с программным запуском. false — таймаут. */
bool adc_read(unsigned channel, uint16_t *out);

/* Обновить показания: температура в 0,1 °C и VDDA в мВ. */
bool adc_sample(int16_t *temp_dc, uint16_t *vdda_mv);

#endif /* ADC_H */
