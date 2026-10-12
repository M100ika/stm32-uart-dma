#!/usr/bin/env python3
"""Утилита для работы с модулем по Modbus RTU с ПК или Raspberry Pi.

Примеры:
    python3 tools/modbus_module.py --port /dev/ttyACM0 status
    python3 tools/modbus_module.py --port COM5 level 250
    python3 tools/modbus_module.py --port /dev/ttyACM0 blink on --period 200
    python3 tools/modbus_module.py --port /dev/ttyACM0 watch
    python3 tools/modbus_module.py --port /dev/ttyACM0 command reset

Нужен пакет minimalmodbus: pip install -r tools/requirements.txt
"""
import argparse
import sys
import time

import minimalmodbus

# Карта регистров — та же, что в src/app.h.
COIL_OUTPUT_ON, COIL_BLINK = 0, 1
DI_BUTTON = 0
(IR_DEVICE_ID, IR_FW_VERSION, IR_UPTIME_HI, IR_UPTIME_LO, IR_BUTTON_PRESSES,
 IR_TEMPERATURE, IR_VDDA_MV, IR_RESET_CAUSE, IR_CRASH_PC_HI, IR_CRASH_PC_LO,
 IR_FRAMES_OK, IR_CRC_ERRORS, IR_EXCEPTIONS, IR_UART_ERRORS, IR_RX_DROPPED) = range(15)
IR_COUNT = 15
HR_OUTPUT_LEVEL, HR_OUTPUT_MODE, HR_BLINK_PERIOD_MS, HR_COMMAND, HR_SCRATCH0 = range(5)

COMMANDS = {"reset": 0xA5A5, "fault": 0xDEAD, "hang": 0x5741}
DEVICES = {0x433: "STM32F401xD/E", 0x421: "STM32F446"}
RESET_CAUSES = ["unknown", "power-on", "reset pin", "software", "independent watchdog",
                "window watchdog", "low-power", "brown-out"]

FN_READ_INPUT = 4


class Module:
    def __init__(self, port, address=1, baudrate=115200, timeout=0.5):
        self.dev = minimalmodbus.Instrument(port, address)
        self.dev.serial.baudrate = baudrate
        self.dev.serial.timeout = timeout
        self.dev.clear_buffers_before_each_transaction = True

    def status(self):
        """Все входные регистры одним запросом — значения согласованы между собой."""
        r = self.dev.read_registers(0, IR_COUNT, functioncode=FN_READ_INPUT)
        temp = r[IR_TEMPERATURE] - 0x10000 if r[IR_TEMPERATURE] & 0x8000 else r[IR_TEMPERATURE]
        return {
            "device": DEVICES.get(r[IR_DEVICE_ID], hex(r[IR_DEVICE_ID])),
            "firmware": f"{r[IR_FW_VERSION] >> 8}.{r[IR_FW_VERSION] & 0xFF}",
            "uptime_s": r[IR_UPTIME_HI] << 16 | r[IR_UPTIME_LO],
            "button_presses": r[IR_BUTTON_PRESSES],
            "temperature_c": temp / 10,
            "vdda_mv": r[IR_VDDA_MV],
            "reset_cause": RESET_CAUSES[r[IR_RESET_CAUSE]] if r[IR_RESET_CAUSE] < len(RESET_CAUSES) else r[IR_RESET_CAUSE],
            "last_crash_pc": hex(r[IR_CRASH_PC_HI] << 16 | r[IR_CRASH_PC_LO]),
            "frames_ok": r[IR_FRAMES_OK],
            "crc_errors": r[IR_CRC_ERRORS],
            "exceptions": r[IR_EXCEPTIONS],
            "uart_errors": r[IR_UART_ERRORS],
            "rx_dropped": r[IR_RX_DROPPED],
            "button_pressed": bool(self.dev.read_bit(DI_BUTTON, functioncode=2)),
            "output_level": self.dev.read_register(HR_OUTPUT_LEVEL),
            "blink": bool(self.dev.read_bit(COIL_BLINK, functioncode=1)),
            "blink_period_ms": self.dev.read_register(HR_BLINK_PERIOD_MS),
        }

    def set_level(self, permille):
        self.dev.write_register(HR_OUTPUT_LEVEL, permille, functioncode=6)

    def set_blink(self, on, period_ms=None):
        if period_ms is None:
            self.dev.write_bit(COIL_BLINK, int(on), functioncode=5)
        else:
            # Режим и период одним запросом 0x10: применятся вместе или никак.
            self.dev.write_registers(HR_OUTPUT_MODE, [int(on), period_ms])

    def command(self, name):
        self.dev.write_register(HR_COMMAND, COMMANDS[name], functioncode=6)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", required=True, help="COM-порт: /dev/ttyACM0, COM5 ...")
    ap.add_argument("--address", type=int, default=1)
    ap.add_argument("--baud", type=int, default=115200)
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    sub.add_parser("watch")
    p = sub.add_parser("level")
    p.add_argument("permille", type=int)
    p = sub.add_parser("blink")
    p.add_argument("state", choices=["on", "off"])
    p.add_argument("--period", type=int)
    p = sub.add_parser("command")
    p.add_argument("name", choices=COMMANDS)
    args = ap.parse_args()

    m = Module(args.port, args.address, args.baud)
    if args.cmd == "status":
        for k, v in m.status().items():
            print(f"{k:16} {v}")
    elif args.cmd == "watch":
        while True:
            s = m.status()
            print(f"\r{s['uptime_s']:>7} s  {s['temperature_c']:5.1f} °C  {s['vdda_mv']} mV  "
                  f"button {'#' if s['button_pressed'] else '.'} x{s['button_presses']}  "
                  f"frames {s['frames_ok']}", end="", flush=True)
            time.sleep(0.5)
    elif args.cmd == "level":
        m.set_level(args.permille)
    elif args.cmd == "blink":
        m.set_blink(args.state == "on", args.period)
    elif args.cmd == "command":
        try:
            m.command(args.name)
        except minimalmodbus.NoResponseError:
            pass  # для fault/hang ответ может не успеть уйти — это ожидаемо
        print("sent; check status after reboot")
    return 0


if __name__ == "__main__":
    sys.exit(main())
