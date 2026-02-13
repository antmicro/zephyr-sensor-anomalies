# Copyright (c) 2026 Antmicro <www.antmicro.com>
#
# SPDX-License-Identifier: Apache-2.0

"""
Python script for generating repl files based on DTS file of the last-built
Zephyr application, with added information about two LIS2DS12 sensors.
"""

from dts2repl import dts2repl
from pathlib import Path
import argparse


def prepare_repl(repl_path):
    with open(repl_path, "w") as repl:
        repl.write(dts2repl.generate("./build/zephyr/zephyr.dts"))

        print("lis2ds12_1: Sensors.LIS2DS12 @ i2c1 0x3d", file=repl)

        print("lis2ds12_2: Sensors.LIS2DS12 @ i2c1 0x2d", file=repl)

    return repl_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--repl", type=Path, help="Stop the simulation after set time")
    args = parser.parse_args()
    prepare_repl(args.repl)
