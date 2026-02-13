# Copyright (c) 2026 Antmicro <www.antmicro.com>
#
# SPDX-License-Identifier: Apache-2.0

"""
Python script for Zephyr applications in Renode and feeding them sensor data.
The script also captures and displays everything printed to the Zephyr console UART.
"""

import argparse
import re
import time
import random
import serial
from pyrenode3.wrappers import Emulation, Analyzer
import csv
from pathlib import Path
from kenning.core.sensor import LIS2DS12Sensor
import tempfile
from gen_repl import prepare_repl


class ZephyrBuildException(Exception):
    """
    Exception thrown for errors caused by an invalid Zephyr application build.
    """

    pass


def get_sensors():
    return [
        LIS2DS12Sensor(platform.sysbus.i2c1.lis2ds12_1),
        LIS2DS12Sensor(platform.sysbus.i2c1.lis2ds12_2),
    ]


def get_cmake_var(cmake_var: str) -> str:
    """
    Retrieves variable from CMake cache.
    """
    with open("./build/CMakeCache.txt", "r") as cache_file:
        cmake_cache = cache_file.read()

    match = re.findall(rf"^{cmake_var}=([^\n\t\s]*)", cmake_cache, re.MULTILINE)
    if len(match):
        return match[0]

    raise ZephyrBuildException(f"{cmake_var} variable not found in CMake cache")


def get_zephyr_console_uart(board: str) -> str:
    """
    Retrieves Zephyr console UART from device tree.
    """
    with open("./build/zephyr/zephyr.dts", "r") as dts_file:
        board_dts = dts_file.read()

    match = re.findall(r"zephyr,console = &([a-zA-Z0-9]*);", board_dts, re.MULTILINE)
    if len(match):
        return match[0]

    raise ZephyrBuildException("Zephyr console UART not found")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--debug", action="store_true", help="Enable GDB server")
    parser.add_argument(
        "--timeout", type=int, help="Stop the simulation after set time"
    )
    parser.add_argument(
        "--frequency", type=int, help="Frequency of feeding sensor samples [Hz]"
    )
    parser.add_argument(
        "--data",
        type=Path,
        help="Path to a file with data to feed to the sensors. If not given, randomized data will be fed.",
    )
    parser.add_argument(
        "--analyzer",
        action="store_true",
        help="Show Renode Analyzer window with the Zephyr console uart output (aside from printing the output to console)",
    )
    args = parser.parse_args()

    board = get_cmake_var("BOARD:STRING").split("/")[0]
    build_path = get_cmake_var("APPLICATION_BINARY_DIR:PATH")
    project_name = get_cmake_var("CMAKE_PROJECT_NAME:STATIC")

    emulation = Emulation()

    platform = emulation.add_mach(board)
    platform.load_repl(prepare_repl(tempfile.mkstemp()[1]))
    platform.load_elf(f"{build_path}/zephyr/zephyr.elf")

    if args.debug:
        platform.StartGdbServer(3333)
        print("gdb server started at :3333")
        print("Press ENTER to start simulation")

        input()

    # create pty terminal for UART with logs
    console_uart = get_zephyr_console_uart(board)
    emulation.CreateUartPtyTerminal("console_uart_term", "/tmp/uart-log")
    emulation.Connector.Connect(
        getattr(platform.sysbus, console_uart).internal,
        emulation.externals.console_uart_term,
    )

    if args.analyzer:
        Analyzer(getattr(platform.sysbus, console_uart)).Show()

    console = serial.Serial("/tmp/uart-log", baudrate=115200)

    print("Starting Renode simulation. Press CTRL+C to exit.")

    dataset_last_sample_time = None

    sensors = get_sensors()

    start_time = time.time()

    logs_tail = ""

    data = None

    if args.data:
        data = []
        with open(args.data, "r") as file:
            parsed_file = csv.reader(file)
            for line in parsed_file:
                data.append(line)
        data = data[1:]  # Skipping the header

    emulation.StartAll()

    data_iter = 0

    while True:
        try:
            if (
                dataset_last_sample_time is None
                or time.time() - dataset_last_sample_time > (1 / args.frequency)
            ):
                dataset_last_sample_time = time.time()
                if data:
                    if data_iter >= len(data):
                        data_iter = 0
                    line = data[data_iter]
                    sample_iter = 0
                    for sensor in sensors:
                        subsample = []
                        for i in range(sensor.size()):
                            subsample.append(float(line[sample_iter]))
                            sample_iter += 1
                        sensor.feed(subsample)
                    data_iter += 1
                else:
                    for sensor in sensors:
                        sensor.feed([random.random() for _ in range(sensor.size())])
            logs = console.read_all().decode(errors="ignore")
            print(logs, end="", flush=True)
            if time.time() - start_time > args.timeout:
                break
        except KeyboardInterrupt:
            print("Exiting...")
            break
        except Exception:
            raise

    console.close()
    emulation.clear()
