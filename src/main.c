/*
 * main.c — модуль ввода-вывода Modbus RTU slave на STM32F401/F446.
 *
 * Мастер (ПЛК, SCADA, Raspberry Pi или ПК) по Modbus RTU читает показания
 * модуля и управляет его выходом через регистры. Линия — USART2:
 * на Nucleo это виртуальный COM-порт ST-LINK, на реальном объекте —
 * приёмопередатчик RS-485 с управлением направлением через PA8.
 * Карта регистров — в app.h и README.
 *
 * Основной цикл: кормит сторожевой таймер, собирает кадр из принятых байт,
 * по паузе t3.5 обрабатывает его и отправляет ответ, обновляет выход
 * и показания датчиков, затем засыпает в WFI до следующего прерывания.
 */
#include <stdint.h>

#include "adc.h"
#include "app.h"
#include "board.h"
#include "clock.h"
#include "cpu.h"
#include "diag.h"
#include "mb_rtu.h"
#include "modbus.h"
#include "pwm.h"
#include "regs.h"
#include "systick.h"
#include "timer_us.h"
#include "uart_dma.h"

#ifndef MB_SLAVE_ADDR
#define MB_SLAVE_ADDR 1U
#endif
#ifndef MB_BAUD
#define MB_BAUD 115200UL
#endif
#define SENSOR_PERIOD_MS 500U

#define DBGMCU_IDCODE (*(volatile uint32_t *)0xE0042000UL)

static app_t s_app;
static mb_rtu_rx_t s_rx;
static mb_counters_t s_mb_cnt;
static uint8_t s_resp[MB_ADU_MAX];

/* Свежие показания для регистров — один снимок на кадр. */
static void refresh_inputs(void)
{
    app_inputs_t *in = &s_app.in;
    in->uptime_s = millis() / 1000U;
    in->button_pressed = button_is_pressed();
    in->button_presses += button_take_presses();
    in->mb = s_mb_cnt;

    uart_stats_t st;
    uart_get_stats(&st);
    in->uart_errors = st.err_overrun + st.err_framing + st.err_noise + st.err_dma;
    in->rx_dropped = st.rx_dropped;
}

static void run_command(app_cmd_t cmd)
{
    if (cmd == APP_CMD_NONE) {
        return;
    }
    /* Ответ мастеру уже в очереди: дожидаемся, пока он уйдёт в линию. */
    uart_flush();
    switch (cmd) {
    case APP_CMD_RESET:
        cpu_reset();
    case APP_CMD_FAULT:
        diag_trigger_fault();
    case APP_CMD_HANG:
        for (;;) {
            /* прерывания работают, но сторожевой таймер никто не кормит */
        }
    default:
        break;
    }
}

static void poll_modbus(void)
{
    uint8_t buf[64];
    size_t n;
    while ((n = uart_read(buf, sizeof(buf))) > 0U) {
        mb_rtu_push(&s_rx, buf, n);
    }

    /*
     * Порядок чтения важен. Сначала текущее время, затем время последнего
     * байта, затем признак непрочитанных данных. Если байт придёт между
     * этими чтениями, он окажется в буфере драйвера и pending будет true —
     * кадр не закроется раньше времени.
     */
    const uint32_t now = timer_us_now();
    const uint32_t last = uart_last_rx_us();
    const bool pending = uart_rx_pending();

    const size_t len = mb_rtu_poll(&s_rx, now, last, pending);
    if (len == 0U) {
        return;
    }
    refresh_inputs();
    const size_t resp_len = mb_process(&s_app.map, MB_SLAVE_ADDR, s_rx.buf, len, s_resp, &s_mb_cnt);
    mb_rtu_reset(&s_rx);
    if (resp_len > 0U) {
        (void)uart_write(s_resp, resp_len);
    }
    run_command(app_take_command(&s_app));
}

int main(void)
{
    const reset_cause_t reset_cause = diag_reset_cause();
    (void)clock_init(); /* при отказе PLL работаем на HSI: частоты ниже берутся фактические */

    systick_init(clock_hclk_hz());
    timer_us_init(clock_apb1_timer_hz());
    pwm_init(clock_apb1_timer_hz());
    button_init();
    adc_init();
    uart_init(clock_pclk1_hz(), MB_BAUD);
    watchdog_init();

    app_init(&s_app);
    s_app.in.device_id = (uint16_t)(DBGMCU_IDCODE & 0xFFFU);
    s_app.in.reset_cause = (uint16_t)reset_cause;
    crash_report_t crash;
    if (diag_take_crash(&crash)) {
        s_app.in.crash_pc = crash.pc;
    }
    mb_rtu_init(&s_rx, MB_BAUD);

    uint32_t last_sensor_ms = 0;
    (void)adc_sample(&s_app.in.temperature_dc, &s_app.in.vdda_mv);

    for (;;) {
        watchdog_feed();
        poll_modbus();

        const uint32_t now_ms = millis();
        pwm_set(app_output_level(&s_app, now_ms));

        if (elapsed(now_ms, last_sensor_ms, SENSOR_PERIOD_MS)) {
            last_sensor_ms = now_ms;
            (void)adc_sample(&s_app.in.temperature_dc, &s_app.in.vdda_mv);
        }

        /* Спим до прерывания: байт по UART, событие DMA или SysTick (1 мс).
         * Паузу t3.5 = 1,75 мс основной цикл замечает с точностью до 1 мс. */
        cpu_wfi();
    }
}
