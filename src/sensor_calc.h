/*
 * sensor_calc.h — пересчёт отсчётов АЦП в милливольты и градусы.
 *
 * Чистая целочисленная арифметика без float: тестируется на ПК и не
 * зависит от FPU. Калибровочные константы ST записывает на заводе
 * при VDDA = 3,3 В, поэтому отсчёты сначала приводятся к 3,3 В.
 */
#ifndef SENSOR_CALC_H
#define SENSOR_CALC_H

#include <stdint.h>

#define CAL_VDDA_MV 3300U

/* VDDA по внутреннему опорному напряжению: VDDA = 3300 * VREFINT_CAL / raw.
 * Возвращает 0, если отсчёт нулевой. */
uint16_t calc_vdda_mv(uint16_t vrefint_raw, uint16_t vrefint_cal);

/* Температура кристалла в десятых долях градуса по двум точкам калибровки
 * (30 и 110 °C). Возвращает INT16_MIN, если калибровка испорчена. */
int16_t calc_temp_dc(uint16_t ts_raw, uint16_t vdda_mv, uint16_t cal30, uint16_t cal110);

#endif /* SENSOR_CALC_H */
