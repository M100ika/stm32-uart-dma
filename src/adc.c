#include "adc.h"

#include "regs.h"
#include "sensor_calc.h"

#define SMP_480_CYCLES 7UL
#define EOC_WAIT_LIMIT 100000UL

void adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;

    /* Тактовая АЦП не выше 36 МГц: APB2 84 МГц / 4 = 21 МГц.
     * TSVREFE подключает датчик температуры и VREFINT к каналам 18 и 17. */
    ADC_COMMON_CCR = ADC_CCR_ADCPRE_DIV4 | ADC_CCR_TSVREFE;

    ADC1->CR1 = 0;            /* 12 бит, без сканирования */
    ADC1->SQR1 = 0;           /* одно преобразование в последовательности */
    /* Датчику температуры нужно не меньше 10 мкс выборки:
     * 480 тактов / 21 МГц = 22,9 мкс. Каналы 10..18 — в SMPR1 по 3 бита. */
    ADC1->SMPR1 = (SMP_480_CYCLES << ((ADC_CH_VREFINT - 10U) * 3U)) |
                  (SMP_480_CYCLES << ((ADC_CH_TEMP - 10U) * 3U));
    ADC1->CR2 = ADC_CR2_ADON;
    /* Первое корректное измерение возможно после стабилизации АЦП и
     * датчика (~10 мкс); первое чтение будет не раньше чем через 500 мс. */
}

bool adc_read(unsigned channel, uint16_t *out)
{
    ADC1->SQR3 = channel;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    for (uint32_t i = 0; i < EOC_WAIT_LIMIT; i++) {
        if (ADC1->SR & ADC_SR_EOC) {
            *out = (uint16_t)ADC1->DR; /* чтение DR сбрасывает EOC */
            return true;
        }
    }
    return false;
}

bool adc_sample(int16_t *temp_dc, uint16_t *vdda_mv)
{
    uint16_t vref_raw, ts_raw;
    if (!adc_read(ADC_CH_VREFINT, &vref_raw) || !adc_read(ADC_CH_TEMP, &ts_raw)) {
        return false;
    }
    *vdda_mv = calc_vdda_mv(vref_raw, VREFINT_CAL);
    *temp_dc = calc_temp_dc(ts_raw, *vdda_mv, TS_CAL1, TS_CAL2);
    return true;
}
