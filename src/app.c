#include "app.h"

#include <string.h>

#define LEVEL_MAX 1000U
#define BLINK_MIN_MS 50U
#define BLINK_MAX_MS 10000U

static bool read_coil(void *ctx, uint16_t addr)
{
    const app_t *app = ctx;
    switch (addr) {
    case COIL_OUTPUT_ON: return app->output_level > 0U;
    case COIL_BLINK:     return app->output_mode == 1U;
    default:             return false;
    }
}

static mb_status_t write_coil(void *ctx, uint16_t addr, bool value, bool commit)
{
    app_t *app = ctx;
    if (!commit) {
        return MB_OK; /* любое значение бита допустимо */
    }
    if (addr == COIL_OUTPUT_ON) {
        app->output_level = value ? LEVEL_MAX : 0U;
    } else if (addr == COIL_BLINK) {
        app->output_mode = value ? 1U : 0U;
    }
    return MB_OK;
}

static bool read_discrete(void *ctx, uint16_t addr)
{
    const app_t *app = ctx;
    return addr == DI_BUTTON && app->in.button_pressed;
}

static uint16_t read_input(void *ctx, uint16_t addr)
{
    const app_inputs_t *in = &((const app_t *)ctx)->in;
    switch (addr) {
    case IR_DEVICE_ID:      return in->device_id;
    case IR_FW_VERSION:     return FW_VERSION_REG;
    case IR_UPTIME_HI:      return (uint16_t)(in->uptime_s >> 16);
    case IR_UPTIME_LO:      return (uint16_t)in->uptime_s;
    case IR_BUTTON_PRESSES: return (uint16_t)in->button_presses;
    case IR_TEMPERATURE:    return (uint16_t)in->temperature_dc; /* дополнительный код */
    case IR_VDDA_MV:        return in->vdda_mv;
    case IR_RESET_CAUSE:    return in->reset_cause;
    case IR_CRASH_PC_HI:    return (uint16_t)(in->crash_pc >> 16);
    case IR_CRASH_PC_LO:    return (uint16_t)in->crash_pc;
    case IR_FRAMES_OK:      return (uint16_t)in->mb.frames_ok;
    case IR_CRC_ERRORS:     return (uint16_t)in->mb.crc_errors;
    case IR_EXCEPTIONS:     return (uint16_t)in->mb.exceptions;
    case IR_UART_ERRORS:    return (uint16_t)in->uart_errors;
    case IR_RX_DROPPED:     return (uint16_t)in->rx_dropped;
    default:                return 0;
    }
}

static uint16_t read_holding(void *ctx, uint16_t addr)
{
    const app_t *app = ctx;
    switch (addr) {
    case HR_OUTPUT_LEVEL:    return app->output_level;
    case HR_OUTPUT_MODE:     return app->output_mode;
    case HR_BLINK_PERIOD_MS: return app->blink_period_ms;
    case HR_COMMAND:         return 0;
    default:
        return (addr >= HR_SCRATCH0 && addr < HR_COUNT) ? app->scratch[addr - HR_SCRATCH0] : 0U;
    }
}

static app_cmd_t command_from_code(uint16_t code)
{
    switch (code) {
    case CMD_CODE_RESET: return APP_CMD_RESET;
    case CMD_CODE_FAULT: return APP_CMD_FAULT;
    case CMD_CODE_HANG:  return APP_CMD_HANG;
    default:             return APP_CMD_NONE;
    }
}

static mb_status_t write_holding(void *ctx, uint16_t addr, uint16_t value, bool commit)
{
    app_t *app = ctx;
    switch (addr) {
    case HR_OUTPUT_LEVEL:
        if (value > LEVEL_MAX) return MB_EX_ILLEGAL_VALUE;
        if (commit) app->output_level = value;
        return MB_OK;
    case HR_OUTPUT_MODE:
        if (value > 1U) return MB_EX_ILLEGAL_VALUE;
        if (commit) app->output_mode = value;
        return MB_OK;
    case HR_BLINK_PERIOD_MS:
        if (value < BLINK_MIN_MS || value > BLINK_MAX_MS) return MB_EX_ILLEGAL_VALUE;
        if (commit) app->blink_period_ms = value;
        return MB_OK;
    case HR_COMMAND:
        if (value != 0U && command_from_code(value) == APP_CMD_NONE) return MB_EX_ILLEGAL_VALUE;
        if (commit) app->pending = command_from_code(value);
        return MB_OK;
    default:
        if (commit) app->scratch[addr - HR_SCRATCH0] = value;
        return MB_OK;
    }
}

void app_init(app_t *app)
{
    memset(app, 0, sizeof(*app));
    app->output_level = LEVEL_MAX;
    app->output_mode = 1;            /* после включения мигает: видно, что модуль жив */
    app->blink_period_ms = 500;

    app->map = (mb_regmap_t){
        .n_coils = COIL_COUNT,
        .n_discrete = DI_COUNT,
        .n_input = IR_COUNT,
        .n_holding = HR_COUNT,
        .read_coil = read_coil,
        .write_coil = write_coil,
        .read_discrete = read_discrete,
        .read_input = read_input,
        .read_holding = read_holding,
        .write_holding = write_holding,
        .ctx = app,
    };
}

uint16_t app_output_level(const app_t *app, uint32_t now_ms)
{
    if (app->output_mode == 1U) {
        const uint32_t phase = (now_ms / app->blink_period_ms) % 2U;
        return phase == 0U ? app->output_level : 0U;
    }
    return app->output_level;
}

app_cmd_t app_take_command(app_t *app)
{
    const app_cmd_t cmd = app->pending;
    app->pending = APP_CMD_NONE;
    return cmd;
}
