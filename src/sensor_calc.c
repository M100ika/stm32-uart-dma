#include "sensor_calc.h"

uint16_t calc_vdda_mv(uint16_t vrefint_raw, uint16_t vrefint_cal)
{
    if (vrefint_raw == 0U) {
        return 0;
    }
    return (uint16_t)(((uint32_t)CAL_VDDA_MV * vrefint_cal + vrefint_raw / 2U) / vrefint_raw);
}

/* Деление с округлением к ближайшему для чисел любого знака (den > 0). */
static int64_t div_round(int64_t num, int64_t den)
{
    return (num >= 0) ? (num + den / 2) / den : (num - den / 2) / den;
}

int16_t calc_temp_dc(uint16_t ts_raw, uint16_t vdda_mv, uint16_t cal30, uint16_t cal110)
{
    if (cal110 <= cal30 || vdda_mv == 0U) {
        return INT16_MIN;
    }
    /*
     * Отсчёт, приведённый к VDDA = 3,3 В:   ts33 = ts_raw * vdda / 3300.
     * Температура в 0,1 °C:                T = 300 + (ts33 - cal30) * 800 / (cal110 - cal30),
     * где 800 = (110 - 30) °C * 10. Чтобы не терять точность на делении,
     * всё сводится к одной дроби. Промежуточные значения доходят до ~10^10,
     * поэтому считаем в 64 битах (4095 * 3600 * 800 уже не влезает в int32).
     */
    const int64_t num = ((int64_t)ts_raw * vdda_mv - (int64_t)cal30 * CAL_VDDA_MV) * 800;
    const int64_t den = (int64_t)(cal110 - cal30) * CAL_VDDA_MV;
    const int64_t temp_dc = 300 + div_round(num, den);
    if (temp_dc <= INT16_MIN || temp_dc > INT16_MAX) {
        return INT16_MIN;
    }
    return (int16_t)temp_dc;
}
