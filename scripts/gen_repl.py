#!/usr/bin/env python3

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
from typing import List
from argparse import ArgumentTypeError


class SensorMapping:
    def __init__(self, name, csharp_class, peripheral, address):
        self.name = name
        self.csharp_class = csharp_class
        self.peripheral = peripheral
        self.address = address
        self.bus = "sysbus"

    def __str__(self):
        return f"{self.name}: {self.csharp_class} @ {self.peripheral} {self.address}"


def parse_sensor(s: str) -> SensorMapping:
    parts = [p.strip() for p in s.split(",")]
    if len(parts) != 4:
        raise ArgumentTypeError("sensor must be 'name,csharp_class,peripheral,address'")
    name, csharp_class, peripheral, addr = parts

    # Address validation. Should be a hex number
    try:
        int(addr, 16)
    except ValueError:
        raise ArgumentTypeError(f"invalid address: {addr}")
    return SensorMapping(name, csharp_class, peripheral, addr)


def prepare_repl(repl_path: str, sensors: List[SensorMapping]) -> str:
    with open(repl_path, "w") as repl:
        repl.write(dts2repl.generate("./build/zephyr/zephyr.dts"))

        for sensor in sensors:
            print(str(sensor), file=repl)

    return repl_path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--repl", type=Path, help="Stop the simulation after set time")
    parser.add_argument(
        "--sensor",
        type=parse_sensor,
        help="Add a new sensor of format 'sensor_name,csharp_class,peripheral,address'",
        action="append",
    )

    # Default sensors
    sensors = [
        SensorMapping("lis2ds12_1", "Sensors.LIS2DS12", "i2c1", "0x3d"),
        SensorMapping("lis2ds12_2", "Sensors.LIS2DS12", "i2c1", "0x2d"),
    ]

    args = parser.parse_args()
    if args.sensor is not None:
        sensors = args.sensor

    names = set(map(lambda s: s.name, sensors))
    addresses = set(map(lambda s: s.address, sensors))

    if len(sensors) > len(names):
        raise ValueError("SensorMapping names are not unique")
    if len(sensors) > len(addresses):
        raise ValueError("SensorMapping addresses are not unique")

    prepare_repl(args.repl, sensors)
