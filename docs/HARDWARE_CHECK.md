# Проверка на плате

Протокол проверен unit-тестами и независимым мастером на симуляторе, но
перед показом прошивку нужно проверить на Nucleo. Ниже порядок проверки
и ожидаемый результат. Обозначение `mm` = `python3 tools/modbus_module.py --port <порт>`.

1. `make flash` (или `make MCU=F446RE flash`), затем **переподключить USB**.
   После прошивки в ядре остаётся включён отладочный режим (бит C_DEBUGEN
   сбрасывается только при включении питания), и обработчик HardFault
   остановился бы на `bkpt`, ожидая отладчик. Светодиод LD2 мигает раз в 0,5 с.
2. `mm status`:
   - `device` — `STM32F401xD/E` (или `STM32F446`), `firmware` — `2.0`;
   - `temperature_c` — 25–45 °C (кристалл теплее комнаты), `vdda_mv` — 3250–3350;
   - `reset_cause` — `power-on` или `reset pin`, `last_crash_pc` — `0x0`.
3. `mm level 50` и `mm blink off` — светодиод горит тускло и не мигает.
   `mm level 1000` — полная яркость. `mm blink on --period 100` — частое мигание.
4. Зажать синюю кнопку и выполнить `mm status`: `button_pressed True`.
   Несколько нажатий — `button_presses` растёт по одному на нажатие.
5. `mm watch` 30 секунд: `frames` растёт, а `crc_errors` и `uart_errors` в
   `mm status` остаются 0.
6. `mm command fault`, затем `mm status`: `reset_cause software`,
   `last_crash_pc 0x8000000` — адрес, куда пытались перейти.
7. `mm command hang`, подождать 5 с, `mm status`: `reset_cause independent watchdog`.
8. `mm command reset`, затем `mm status`: `reset_cause software`, `uptime_s` начался заново.
9. Сторонний мастер (QModMaster или Modbus Poll): адрес 1, 115200 8N1, прочитать
   input registers 0–14 и записать holding register 0 — результат тот же.
10. По желанию: осциллограф или логический анализатор на PA8 и PA2. DE поднимается
    до первого стартового бита ответа и опускается после последнего стоп-бита.
