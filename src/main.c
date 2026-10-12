/*
 * main.c — консоль на USART2 (виртуальный COM-порт ST-LINK, 115200 8N1).
 *
 * Основной цикл: кормит сторожевой таймер, разбирает принятые байты
 * командной строкой, сообщает о нажатиях кнопки, мигает светодиодом
 * и засыпает в WFI до следующего прерывания (не реже раза в 1 мс — SysTick).
 */
#include <stdint.h>
#include <string.h>

#include "board.h"
#include "cli.h"
#include "clock.h"
#include "cpu.h"
#include "diag.h"
#include "regs.h"
#include "strutil.h"
#include "systick.h"
#include "uart_dma.h"

#ifndef MCU_NAME
#define MCU_NAME "STM32F4"
#endif
#define FW_VERSION "1.0.0"
#define UART_BAUD 115200UL
#define BLINK_PERIOD_MS 500U

#define DBGMCU_IDCODE (*(volatile uint32_t *)0xE0042000UL)

static cli_t s_cli;
static bool s_led_auto = true;
static uint32_t s_button_total;

static void put_num(const char *label, uint32_t value)
{
    char num[U32_DEC_MAX];
    u32_to_dec(num, value);
    uart_puts(label);
    uart_puts(num);
    uart_puts("\r\n");
}

static void put_hex(const char *label, uint32_t value)
{
    char num[U32_HEX_MAX];
    u32_to_hex(num, value);
    uart_puts(label);
    uart_puts(num);
    uart_puts("\r\n");
}

/* ------------------------------------------------------------- команды */

static void cmd_led(cli_t *cli, int argc, char *argv[])
{
    if (argc != 2) {
        cli_puts(cli, "usage: led on|off|toggle|auto\r\n");
        return;
    }
    if (strcmp(argv[1], "auto") == 0) {
        s_led_auto = true;
        return;
    }
    s_led_auto = false;
    if (strcmp(argv[1], "on") == 0) {
        led_set(true);
    } else if (strcmp(argv[1], "off") == 0) {
        led_set(false);
    } else if (strcmp(argv[1], "toggle") == 0) {
        led_toggle();
    } else {
        cli_puts(cli, "usage: led on|off|toggle|auto\r\n");
    }
}

static void cmd_uptime(cli_t *cli, int argc, char *argv[])
{
    (void)cli;
    (void)argc;
    (void)argv;
    const uint32_t ms = millis();
    put_num("uptime, s: ", ms / 1000U);
}

static void cmd_stats(cli_t *cli, int argc, char *argv[])
{
    (void)cli;
    (void)argc;
    (void)argv;
    uart_stats_t st;
    uart_get_stats(&st);
    put_num("rx bytes:      ", st.rx_bytes);
    put_num("rx dropped:    ", st.rx_dropped);
    put_num("tx bytes:      ", st.tx_bytes);
    put_num("tx dropped:    ", st.tx_dropped);
    put_num("overrun err:   ", st.err_overrun);
    put_num("framing err:   ", st.err_framing);
    put_num("noise err:     ", st.err_noise);
    put_num("dma err:       ", st.err_dma);
    put_num("button presses:", s_button_total);
}

static void cmd_echo(cli_t *cli, int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        cli_puts(cli, argv[i]);
        cli_puts(cli, i + 1 < argc ? " " : "");
    }
    cli_puts(cli, "\r\n");
}

static void cmd_info(cli_t *cli, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_puts(cli, "firmware: " FW_VERSION ", built " __DATE__ " " __TIME__ "\r\n");
    cli_puts(cli, "target:   " MCU_NAME "\r\n");
    put_hex("dev id:   ", DBGMCU_IDCODE & 0xFFFU); /* 0x433 — F401xD/E, 0x421 — F446 */
    put_num("hclk, Hz: ", clock_hclk_hz());
    put_num("pclk1,Hz: ", clock_pclk1_hz());
}

static void cmd_reset(cli_t *cli, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_puts(cli, "resetting...\r\n");
    uart_flush();
    cpu_reset();
}

static void cmd_fault(cli_t *cli, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_puts(cli, "triggering HardFault, report will be shown after reboot\r\n");
    uart_flush();
    diag_trigger_fault();
}

static void cmd_hang(cli_t *cli, int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    cli_puts(cli, "main loop stopped, watchdog will reset in ~2 s\r\n");
    for (;;) {
        /* прерывания работают, но сторожевой таймер никто не кормит */
    }
}

static const cli_cmd_t COMMANDS[] = {
    { "led",    "led on|off|toggle|auto", cmd_led },
    { "uptime", "time since boot",        cmd_uptime },
    { "stats",  "uart and dma counters",  cmd_stats },
    { "echo",   "echo <text>",            cmd_echo },
    { "info",   "firmware and clocks",    cmd_info },
    { "reset",  "software reset",         cmd_reset },
    { "fault",  "trigger HardFault",      cmd_fault },
    { "hang",   "stop feeding watchdog",  cmd_hang },
};

/* ------------------------------------------------------------- запуск */

static void print_banner(bool pll_ok, const char *reset_cause)
{
    uart_puts("\r\n\r\nstm32-uart-dma " FW_VERSION " (" MCU_NAME ")\r\n");
    uart_puts("reset cause: ");
    uart_puts(reset_cause);
    uart_puts("\r\n");
    if (!pll_ok) {
        uart_puts("warning: PLL failed, running on HSI 16 MHz\r\n");
    }

    crash_report_t crash;
    if (diag_take_crash(&crash)) {
        uart_puts("previous run ended with HardFault:\r\n");
        put_hex("  pc   ", crash.pc);
        put_hex("  lr   ", crash.lr);
        put_hex("  psr  ", crash.psr);
        put_hex("  cfsr ", crash.cfsr);
        put_hex("  hfsr ", crash.hfsr);
        put_hex("  mmfar", crash.mmfar);
        put_hex("  bfar ", crash.bfar);
    }
    uart_puts("type help for commands\r\n");
}

int main(void)
{
    const char *reset_cause = diag_reset_cause();
    const bool pll_ok = clock_init();

    systick_init(clock_hclk_hz());
    led_init();
    button_init();
    uart_init(clock_pclk1_hz(), UART_BAUD);
    watchdog_init();

    cli_init(&s_cli, COMMANDS, sizeof(COMMANDS) / sizeof(COMMANDS[0]), uart_puts);
    print_banner(pll_ok, reset_cause);
    cli_prompt(&s_cli);

    uint32_t last_blink = millis();

    for (;;) {
        watchdog_feed();

        uint8_t buf[32];
        size_t n;
        while ((n = uart_read(buf, sizeof(buf))) > 0U) {
            for (size_t i = 0; i < n; i++) {
                cli_feed(&s_cli, (char)buf[i]);
            }
        }

        const uint32_t presses = button_take_presses();
        if (presses > 0U) {
            s_button_total += presses;
            uart_puts("\r\n[button] pressed, total ");
            char num[U32_DEC_MAX];
            u32_to_dec(num, s_button_total);
            uart_puts(num);
            uart_puts("\r\n");
            cli_redraw(&s_cli);
        }

        const uint32_t now = millis();
        if (s_led_auto && elapsed(now, last_blink, BLINK_PERIOD_MS)) {
            last_blink = now;
            led_toggle();
        }

        /* Спим до прерывания. Если байт придёт между uart_read и WFI,
         * его обработка задержится не больше чем на 1 мс до SysTick. */
        cpu_wfi();
    }
}
