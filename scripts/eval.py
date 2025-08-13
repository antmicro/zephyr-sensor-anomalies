#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "dts2repl",
#     "pyrenode3[all]",
#     "bpython",
# ]
#
# [tool.uv.sources]
# dts2repl = { git = "https://github.com/antmicro/dts2repl.git" }
# pyrenode3 = { git = "https://github.com/antmicro/pyrenode3.git" }
# ///
import pyrenode3  # noqa: F401

from pyrenode3.wrappers import Emulation, Analyzer
from System import Decimal
from dts2repl import dts2repl
import bpython

import tempfile
import threading
import math
import time


def prepare_repl():
    global repl_temp_file
    repl_temp_file = tempfile.NamedTemporaryFile(
        "w", delete=True, delete_on_close=False, suffix=".repl"
    )

    repl_temp_file.write(dts2repl.generate("./build/zephyr/zephyr.dts"))

    print("lis2ds12: Sensors.LIS2DS12 @ i2c1 0x3d", file=repl_temp_file)

    print("lis2ds12_2: Sensors.LIS2DS12 @ i2c1 0x2d", file=repl_temp_file)

    repl_temp_file.close()

    return repl_temp_file.name


def prepare_machine(emu, repl):
    machine = emu.add_mach()
    machine.load_repl(repl)
    machine.load_elf("build/zephyr/zephyr.elf")

    return machine


def feed_sensor(time_fn, feeder_fn, delay):
    previous_time = time_fn()
    while True:
        time.sleep(0.1)
        current_time = time_fn()
        if current_time - previous_time > delay:
            previous_time = current_time
            feeder_fn(current_time)


def main():
    emu = Emulation()
    repl = prepare_repl()
    machine = prepare_machine(emu, repl)

    Analyzer(machine.sysbus.usart1).Show()

    emu.StartAll()

    machine.sysbus.i2c1.lis2ds12.AccelerationX = Decimal(8)

    machine.sysbus.i2c1.lis2ds12_2.AccelerationZ = Decimal(3.5)

    def time_fn():
        return machine.ElapsedVirtualTime.TimeElapsed.TotalSeconds

    def feeder_fn_wrp(s, n):
        def feeder_fn(t):
            setattr(s, n, Decimal(math.sin(t * 2 * 3.1415)))

        return feeder_fn

    threads = [
        threading.Thread(
            target=lambda: feed_sensor(
                time_fn,
                feeder_fn_wrp(machine.sysbus.i2c1.lis2ds12, "AccelerationX"),
                0.05,
            )(),
            daemon=True,
        ),
        threading.Thread(
            target=lambda: feed_sensor(
                time_fn,
                feeder_fn_wrp(machine.sysbus.i2c1.lis2ds12, "AccelerationY"),
                0.05,
            )(),
            daemon=True,
        ),
    ]

    for t in threads:
        t.start()

    bpython.embed({**globals(), **locals()})


if __name__ == "__main__":
    main()
