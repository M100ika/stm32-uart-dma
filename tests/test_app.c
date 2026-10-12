#include "app.h"
#include "test.h"

static app_t s_app;

static mb_status_t write_hr(uint16_t addr, uint16_t v)
{
    mb_status_t st = s_app.map.write_holding(s_app.map.ctx, addr, v, false);
    if (st == MB_OK) {
        st = s_app.map.write_holding(s_app.map.ctx, addr, v, true);
    }
    return st;
}

static void defaults_blink_at_full_level(void)
{
    app_init(&s_app);
    CHECK_EQ(s_app.map.read_holding(s_app.map.ctx, HR_OUTPUT_LEVEL), 1000);
    CHECK_EQ(s_app.map.read_holding(s_app.map.ctx, HR_OUTPUT_MODE), 1);
    CHECK_EQ(s_app.map.read_holding(s_app.map.ctx, HR_BLINK_PERIOD_MS), 500);
    CHECK(s_app.map.read_coil(s_app.map.ctx, COIL_BLINK));
    CHECK_EQ(s_app.map.read_input(s_app.map.ctx, IR_FW_VERSION), FW_VERSION_REG);
}

static void blink_follows_time(void)
{
    app_init(&s_app);
    write_hr(HR_OUTPUT_LEVEL, 300);
    write_hr(HR_BLINK_PERIOD_MS, 100);
    CHECK_EQ(app_output_level(&s_app, 0), 300);
    CHECK_EQ(app_output_level(&s_app, 99), 300);
    CHECK_EQ(app_output_level(&s_app, 100), 0);
    CHECK_EQ(app_output_level(&s_app, 250), 300);
    write_hr(HR_OUTPUT_MODE, 0);
    CHECK_EQ(app_output_level(&s_app, 150), 300);
}

static void coils_drive_output(void)
{
    app_init(&s_app);
    s_app.map.write_coil(s_app.map.ctx, COIL_BLINK, false, true);
    s_app.map.write_coil(s_app.map.ctx, COIL_OUTPUT_ON, false, true);
    CHECK_EQ(app_output_level(&s_app, 0), 0);
    CHECK(!s_app.map.read_coil(s_app.map.ctx, COIL_OUTPUT_ON));
    s_app.map.write_coil(s_app.map.ctx, COIL_OUTPUT_ON, true, true);
    CHECK_EQ(app_output_level(&s_app, 0), 1000);
}

static void holding_validation(void)
{
    app_init(&s_app);
    CHECK_EQ(write_hr(HR_OUTPUT_LEVEL, 1001), MB_EX_ILLEGAL_VALUE);
    CHECK_EQ(write_hr(HR_OUTPUT_MODE, 2), MB_EX_ILLEGAL_VALUE);
    CHECK_EQ(write_hr(HR_BLINK_PERIOD_MS, 49), MB_EX_ILLEGAL_VALUE);
    CHECK_EQ(write_hr(HR_BLINK_PERIOD_MS, 10001), MB_EX_ILLEGAL_VALUE);
    CHECK_EQ(write_hr(HR_BLINK_PERIOD_MS, 50), MB_OK);
    CHECK_EQ(write_hr(HR_COMMAND, 0x1234), MB_EX_ILLEGAL_VALUE);
    CHECK_EQ(s_app.map.read_holding(s_app.map.ctx, HR_OUTPUT_LEVEL), 1000); /* не изменился */
}

static void commands_are_deferred(void)
{
    app_init(&s_app);
    CHECK_EQ(app_take_command(&s_app), APP_CMD_NONE);
    CHECK_EQ(write_hr(HR_COMMAND, CMD_CODE_RESET), MB_OK);
    CHECK_EQ(s_app.map.read_holding(s_app.map.ctx, HR_COMMAND), 0);
    CHECK_EQ(app_take_command(&s_app), APP_CMD_RESET);
    CHECK_EQ(app_take_command(&s_app), APP_CMD_NONE); /* выполняется один раз */
    write_hr(HR_COMMAND, CMD_CODE_HANG);
    CHECK_EQ(app_take_command(&s_app), APP_CMD_HANG);
}

static void scratch_registers(void)
{
    app_init(&s_app);
    for (uint16_t i = 0; i < 8; i++) {
        CHECK_EQ(write_hr((uint16_t)(HR_SCRATCH0 + i), (uint16_t)(0xBEE0 + i)), MB_OK);
    }
    CHECK_EQ(s_app.map.read_holding(s_app.map.ctx, HR_SCRATCH0 + 7), 0xBEE7);
    CHECK_EQ(s_app.map.n_holding, HR_SCRATCH0 + 8);
}

static void inputs_split_32bit_values(void)
{
    app_init(&s_app);
    s_app.in.uptime_s = 0x00012345U;
    s_app.in.crash_pc = 0x08000ABCU;
    s_app.in.temperature_dc = -125;
    CHECK_EQ(s_app.map.read_input(s_app.map.ctx, IR_UPTIME_HI), 0x0001);
    CHECK_EQ(s_app.map.read_input(s_app.map.ctx, IR_UPTIME_LO), 0x2345);
    CHECK_EQ(s_app.map.read_input(s_app.map.ctx, IR_CRASH_PC_HI), 0x0800);
    CHECK_EQ(s_app.map.read_input(s_app.map.ctx, IR_CRASH_PC_LO), 0x0ABC);
    CHECK_EQ(s_app.map.read_input(s_app.map.ctx, IR_TEMPERATURE), 0xFF83); /* -125 */
}

static void full_request_through_modbus(void)
{
    /* Запрос мастера целиком: записать уровень 250 в регистр 0 и прочитать
     * три первых holding-регистра. */
    app_init(&s_app);
    mb_counters_t cnt = { 0 };
    uint8_t resp[MB_ADU_MAX];
    uint8_t wr[8] = { 1, 0x06, 0x00, HR_OUTPUT_LEVEL, 0x00, 250 };
    uint16_t crc = mb_crc16(wr, 6);
    wr[6] = (uint8_t)crc;
    wr[7] = (uint8_t)(crc >> 8);
    CHECK_EQ(mb_process(&s_app.map, 1, wr, 8, resp, &cnt), 8);

    uint8_t rd[8] = { 1, 0x03, 0x00, 0x00, 0x00, 0x03 };
    crc = mb_crc16(rd, 6);
    rd[6] = (uint8_t)crc;
    rd[7] = (uint8_t)(crc >> 8);
    CHECK_EQ(mb_process(&s_app.map, 1, rd, 8, resp, &cnt), 11);
    CHECK_EQ(resp[4], 250);       /* уровень */
    CHECK_EQ(resp[6], 1);         /* режим мигания */
    CHECK_EQ(resp[7] << 8 | resp[8], 500);
}

void run_app_tests(void)
{
    RUN(defaults_blink_at_full_level);
    RUN(blink_follows_time);
    RUN(coils_drive_output);
    RUN(holding_validation);
    RUN(commands_are_deferred);
    RUN(scratch_registers);
    RUN(inputs_split_32bit_values);
    RUN(full_request_through_modbus);
}
