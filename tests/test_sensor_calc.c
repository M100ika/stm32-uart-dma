#include <stdint.h>

#include "sensor_calc.h"
#include "test.h"

/* Калибровка, близкая к реальной: 4 отсчёта на градус. */
#define CAL30  940U
#define CAL110 1260U

static void vdda_from_vrefint(void)
{
    CHECK_EQ(calc_vdda_mv(1500, 1500), 3300);  /* питание ровно 3,3 В */
    CHECK_EQ(calc_vdda_mv(1650, 1500), 3000);  /* питание ниже — отсчёт выше */
    CHECK_EQ(calc_vdda_mv(0, 1500), 0);
}

static void temperature_at_calibration_points(void)
{
    CHECK_EQ(calc_temp_dc(CAL30, 3300, CAL30, CAL110), 300);
    CHECK_EQ(calc_temp_dc(CAL110, 3300, CAL30, CAL110), 1100);
    CHECK_EQ(calc_temp_dc((CAL30 + CAL110) / 2, 3300, CAL30, CAL110), 700);
}

static void temperature_below_zero(void)
{
    CHECK_EQ(calc_temp_dc(CAL30 - 40, 3300, CAL30, CAL110), 200);   /* 20,0 °C */
    CHECK_EQ(calc_temp_dc(CAL30 - 160, 3300, CAL30, CAL110), -100); /* -10,0 °C */
    CHECK_EQ(calc_temp_dc(CAL30 - 2, 3300, CAL30, CAL110), 295);    /* 29,5 °C */
}

static void temperature_compensates_vdda(void)
{
    /* При VDDA = 3,0 В тот же датчик даёт отсчёт в 1,1 раза больше. */
    CHECK_EQ(calc_temp_dc(1034, 3000, CAL30, CAL110), 300);
}

static void no_overflow_at_extremes(void)
{
    /* Максимальный отсчёт и высокое VDDA: промежуточное значение больше 2^31.
     * В 32 битах оно бы переполнилось; в 64 битах результат точный. */
    CHECK_EQ(calc_temp_dc(4095, 3600, CAL30, CAL110), 9118);
    /* Результат, не влезающий в int16, — признак ошибки, а не мусор. */
    CHECK_EQ(calc_temp_dc(4095, 3600, 940, 941), INT16_MIN);
}

static void broken_calibration(void)
{
    CHECK_EQ(calc_temp_dc(1000, 3300, 1200, 1200), INT16_MIN);
    CHECK_EQ(calc_temp_dc(1000, 3300, 1300, 1200), INT16_MIN);
    CHECK_EQ(calc_temp_dc(1000, 0, CAL30, CAL110), INT16_MIN);
}

void run_sensor_calc_tests(void)
{
    RUN(vdda_from_vrefint);
    RUN(temperature_at_calibration_points);
    RUN(temperature_below_zero);
    RUN(temperature_compensates_vdda);
    RUN(no_overflow_at_extremes);
    RUN(broken_calibration);
}
