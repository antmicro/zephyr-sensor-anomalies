# Kenning Zephyr Sensor Anomalies

Copyright (c) 2025 [Antmicro](https://www.antmicro.com)

## Installing dependencies
Install [Zephyr dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies):

```shell skip
apt install --no-install-recommends git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
```

then, install `west` with `pip`:

```shell
pip install west
```

and initialize Zephyr:

```shell
west init -l .
west update
west patch apply
west zephyr-export
west packages pip --install
west sdk install -t arm-zephyr-eabi
```

## Running samples

To build `sensors` sample for `stm32f746g_disco` platform use:

```shell
west build -p -b stm32f746g_disco samples/sensors
```

Download Renode and activate it using the included scripts:

```shell
./scripts/prepare_renode.sh
source ./scripts/activate_renode.sh
```

Run the sample in Renode:

```shell
./scripts/eval.py
```
