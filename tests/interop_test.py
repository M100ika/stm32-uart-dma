#!/usr/bin/env python3
"""Проверка протокола независимой реализацией Modbus-мастера (minimalmodbus).

Запускает симулятор (тот же код протокола и карты регистров, что в прошивке)
на псевдотерминале и гоняет через него запросы всех поддерживаемых функций.

    make interop
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools"))

import minimalmodbus  # noqa: E402
import modbus_module as mm  # noqa: E402

SIM = sys.argv[1] if len(sys.argv) > 1 else "build/host/slave_sim"

failures = 0


def check(name, cond):
    global failures
    print(f"  {'ok  ' if cond else 'FAIL'} {name}")
    if not cond:
        failures += 1


def expect_exception(name, fn, code_cls):
    try:
        fn()
    except code_cls:
        check(name, True)
        return
    except Exception as e:  # noqa: BLE001
        check(f"{name} (got {type(e).__name__}: {e})", False)
        return
    check(f"{name} (no exception)", False)


def main():
    sim = subprocess.Popen([SIM], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    try:
        port = sim.stdout.readline().strip()
        m = mm.Module(port, address=1, timeout=1.0)
        dev = m.dev

        s = m.status()
        check("FC04 read input registers: device id", s["device"] == "STM32F401xD/E")
        check("FC04 firmware version", s["firmware"] == "2.0")
        check("FC04 signed temperature", s["temperature_c"] == 24.5)
        check("FC02 read discrete input", s["button_pressed"] is False)
        check("defaults: blink on, 500 ms", s["blink"] and s["blink_period_ms"] == 500)

        m.set_level(250)
        check("FC06 write + FC03 read holding", dev.read_register(mm.HR_OUTPUT_LEVEL) == 250)

        dev.write_bit(mm.COIL_OUTPUT_ON, 0, functioncode=5)
        check("FC05 write coil + FC01 read coil", dev.read_bit(mm.COIL_OUTPUT_ON, functioncode=1) == 0)
        check("coil changes holding register", dev.read_register(mm.HR_OUTPUT_LEVEL) == 0)

        m.set_blink(True, 120)
        check("FC10 write multiple registers",
              dev.read_registers(mm.HR_OUTPUT_MODE, 2) == [1, 120])

        values = [0x1111 * (i + 1) for i in range(8)]
        dev.write_registers(mm.HR_SCRATCH0, values)
        check("FC10 + FC03 scratch registers", dev.read_registers(mm.HR_SCRATCH0, 8) == values)

        expect_exception("value out of range -> exception 03",
                         lambda: dev.write_register(mm.HR_OUTPUT_LEVEL, 1001, functioncode=6),
                         minimalmodbus.IllegalRequestError)
        expect_exception("address out of range -> exception 02",
                         lambda: dev.read_register(100),
                         minimalmodbus.IllegalRequestError)
        expect_exception("atomic multi-write rejects invalid value",
                         lambda: dev.write_registers(mm.HR_OUTPUT_LEVEL, [500, 7]),
                         minimalmodbus.IllegalRequestError)
        check("nothing applied after rejected multi-write",
              dev.read_register(mm.HR_OUTPUT_LEVEL) == 0)

        # Запрос к другому адресу: модуль должен промолчать.
        other = mm.Module(port, address=7, timeout=0.3).dev
        expect_exception("other slave address -> no response",
                         lambda: other.read_register(0),
                         minimalmodbus.NoResponseError)

        m.command("reset")
        check("command register accepted", dev.read_register(mm.HR_COMMAND) == 0)

        s = m.status()
        check("frame counter counts requests", s["frames_ok"] > 10)
        check("no CRC errors on clean line", s["crc_errors"] == 0)
        check("exceptions counted", s["exceptions"] == 3)
    finally:
        sim.kill()
        sim.wait()

    print(f"\n{failures} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
