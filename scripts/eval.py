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
from dts2repl import dts2repl
import bpython

import tempfile


def prepare_repl():
    global repl_temp_file
    repl_temp_file = tempfile.NamedTemporaryFile(
        "w", delete=True, delete_on_close=False, suffix=".repl"
    )

    repl_temp_file.write(dts2repl.generate("./build/zephyr/zephyr.dts"))

    print("lis2ds12_1: Sensors.LIS2DS12 @ i2c1 0x3d", file=repl_temp_file)

    print("lis2ds12_2: Sensors.LIS2DS12 @ i2c1 0x2d", file=repl_temp_file)

    repl_temp_file.close()

    return repl_temp_file.name


def prepare_machine(emu, repl):
    machine = emu.add_mach()
    machine.load_repl(repl)
    machine.load_elf("build/zephyr/zephyr.elf")

    return machine


def main():
    emu = Emulation()
    repl = prepare_repl()
    machine = prepare_machine(emu, repl)

    Analyzer(machine.sysbus.usart1).Show()

    machine.sysbus.i2c1.lis2ds12_1.FeedSample("samples/sensors/input1.data", -1)

    machine.sysbus.i2c1.lis2ds12_2.FeedSample("samples/sensors/input2.data", -1)

    emu.StartAll()

    bpython.embed({**globals(), **locals()})


if __name__ == "__main__":
    main()
