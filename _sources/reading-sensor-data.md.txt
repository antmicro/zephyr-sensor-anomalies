# Reading Sensor Data

This tutorial outlines how to build a C application from scratch using `zephyr-sensor-anomalies` to read sensor data from the [LIS2DS12](https://www.st.com/en/mems-and-sensors/lis2ds12.html) accelerometer on the [STM32F746NG board](https://www.stour.com/en/evaluation-tools/32f746gdiscovery.html).

## Quickstart Example
Ensure that you have taken the necessary steps to [install all dependencies](getting-started).
This tutorial assumes that you have installed `west` and have the `uv` virtual environment in place.

```bash
# If you have not yet activated the virtual environment:
source .venv/bin/activate
# Create our private workspace:
mkdir -p sensor_app && cd sensor_app
```

In the `sensor_app/` directory, create the following files:

```
sensor_app/
    boards/
        stm32f746g_disco.overlay
    src/
        main.c
    CMakeLists.txt
    prj.conf
```

This tutorial shows how to write a simple Zephyr application that prints out the obtained values of two accelerometers attached to the board.
Below are the full contents of `main.c`:

```c
#include <zephyr/kernel.h>

#include <stdio.h>

#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

#define CSV_READER_MAX_ROW_SIZE 128

int main() {
    static struct provider_reader pr = {0};

    static float buffer[CSV_READER_MAX_ROW_SIZE] = {0};
    static struct provider_hdr_entry hdr[CSV_READER_MAX_ROW_SIZE] = {0};

    int ret = provider_reader_register_all_sensor(&pr);
    provider_reader_hdr(&pr, hdr);

    if (sizeof buffer / sizeof *buffer < pr.dst_N) {
        printk("Insufficient buffer size: %u < %u", sizeof buffer / sizeof *buffer, pr.dst_N);
        return -1;
    }

    /* Fill in the headers */
    for (int i = 0; i < pr.dst_N; ++i) {
        if (i > 0) {
            printk(",");
        }
        printk("%u_%u", hdr[i].provider_id, hdr[i].chan);
    }
    printk("\n");

    while (1) {
        provider_reader_read_all(&pr, buffer);

        /* Print out the results */
        for (int i = 0; i < pr.dst_N; ++i) {
            if (i > 0) {
                printk(",")
            }
            printk("%f", (double) buffer[i]);
        }
        printk("\n");

        k_msleep(200); /* Sleep for 200 ms */
    }

    return 0;
}
```
### Step-by-step of the code example

This subsection will go into the code in more detail.

The code below initializes the primary struct for the provider reader context.
This struct contains: `pr.ps_N`, which is the _number of providers_ and `pr.dst_N`, which is the _total number of channels_.
For example, if there are two providers, one with two channels and another with three, then `pr.dst_N` is set to `5` by this library.

When reading from a provider, the obtained sensor data is saved into `buffer[]`.
Ensure that there is enough memory that can hold the data from all sensors.
This buffer is overwritten whenever the sensors are sampled; callers must copy the sensor data if persistence is required.

The third line is an array of the headers of each provider.
This contains the `hdr.provider_id` member variable and the `hdr.chan`, which indicates which channel it is.

```c
static struct provider_reader pr = {0};
static float buffer[CSV_READER_MAX_ROW_SIZE] = {0};
static struct provider_hdr_entry hdr[CSV_READER_MAX_ROW_SIZE] = {0};
```

This line attempts to register all the available sensors stated in the config file `kenning-zephyr-sensor-anomalies/lib/provider_lib/sensor_map.yaml`:

```c
int ret = provider_reader_register_all_sensors(&pr);
```

The config file contains the name of the sensor and its channels:

```yaml
- st_lis2ds12:
  - ACCEL_X
  - ACCEL_Y
  - ACCEL_Z

- adi_axl345:
  - ACCEL_X
  - ACCEL_Y
  - ACCEL_Z
```

The name of the sensor maps to the `compatible` field in each of the sensor in the DTS overlay with the `,` replaced with `_`.
In this instance, `st_lis2ds12` is the sensor name which maps to `st,lis2ds12` in the DTS overlay file.
More information about the DTS overlay can be found in the [Zephyr documentation](https://docs.zephyrproject.org/latest/build/dts/howtos.html).
The DTS overlay is laid out in `boards/stm32f746g_disco.overlay`:

```dts
&i2c1 {
    lis2ds12N1: lis2ds12@3d {
        compatible = "st,lis2ds12";
        reg = <0x3d>;
        status = "okay";
    };
    lis2ds12N2: lis2ds12@2d {
        compatible = "st,lis2ds12";
        reg = <0x2d>;
        status = "okay";
    };
};
```

Once the sensors are registered, the function `provider_reader_hdr` reads the headers.
This action is achieved in the following lines:

```c
provider_reader_hdr(&pr, hdr);

for (int i = 0; i < pr.dst_N; ++i) {
    if (i > 0) {
        printk(",");
    }
    printk("%u_%u", hdr[i].provider_id, hdr[i].chan);
}
```

Now that the headers are in place, the values can be obtained.
The next loop reads the current values of all sensors' channels into the buffer:

```c
provider_reader_read_all(&pr, buffer);

/* Print out the results */
for (int i = 1; i < pr.dst_N; ++i) {
    if (i > 0) {
        printk(",");
    }
    printk("%f", (double) buffer[i]);
}
```


Before the application can be built, it requires a `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.13)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(app LANGUAGES C)

target_sources(app PRIVATE src/main.c)
```

and `prj.conf`:

```conf
CONFIG_SENSOR=y # enable zephyr sensors
CONFIG_PROVIDER_LIB=y # build our library
CONFIG_LIS2DS12=y # build the drivers for the LIS2DS12 sensor
CONFIG_CBPRINTF_FP_SUPPORT=y
```

To build the project, execute:

```bash
west build -p -b stm32f746g_disco sensor_app
```

## Simulating on Renode
To simulate the application on Renode, first download Renode.
The script for that is provided in the source tree:

```bash
./scripts/prepare_renode
source ./scripts/activate_renode.sh
```

Sample data has to be provided in order to fed it to the emulated sensors.
Your own data can also be provided.
```bash
curl https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv -o data.csv
```

To run in Renode (the emulation terminates after 5 seconds):

```bash
python ./scripts/run_renode.py \
    --timeout 5 \
    --frequency 100 \
    --data data.csv
```

You should see a similar output to the one below:
```
Starting Renode simulation. Press CTRL+C to exit.
*** Booting Zephyr OS build v4.2.2-1-g17081cb00713 ***
0_0,0_1,0_2,1_0,1_1,1_2
0.000000,0.000000,-0.008374,0.063409,-0.013160,0.000000
0.000000,-0.004785,-0.008374,-0.210568,-0.014356,0.000000
-0.037088,-0.041874,-0.453439,-0.439082,0.331405,0.001196
0.004785,-0.003589,-0.128016,0.081355,0.124426,-0.027517
0.005982,-0.003589,-0.075373,-0.003589,-0.086141,0.001196
0.001196,0.000000,-0.195015,0.153140,0.340977,-0.032303
-0.004785,0.001196,0.177068,0.076570,-0.080159,-0.105284
-0.017946,-0.022731,-0.124426,-0.362512,-0.118444,0.001196
-0.003589,0.000000,0.044267,-0.013160,-0.095712,0.005982
0.005982,0.001196,-0.017946,-0.028713,-0.057427,0.004785
-0.008374,0.004785,-0.051445,0.038285,-0.028713,0.004785
```

## Using a different sensor

For now, we have been performing an example that is considered default by the `samples/sensors` directory.
This section presents how to use a different sensor; namely, [ADXL345](https://www.analog.com/en/products/adxl345.html).

First, modify the `stm32f746g_disco.overlay` file:
```dts
&i2c1 {
    adi_adxl345N1: adi_adxl345@3d {
        compatible = "adi,adxl345";
        reg = <0x3d>;
        status = "okay";
    };
    adi_adxl345N2: adi_adxl345@2d {
        compatible = "adi,adxl345";
        reg = <0x2d>;
        status = "okay";
    };
};
```

Next, modify the `prj.conf` file:

```
CONFIG_SENSOR=y
CONFIG_PROVIDER_LIB=y
CONFIG_ADXL345=y
CONFIG_CBPRINTF_FP_SUPPORT=y
```

Rebuild the project using the command below:

```bash
west build -b stm32f746g_disco sensor_app
```

Renode needs to be informed of the specific sensors by passing the `--sensor` flag to `run_renode.py` which has the following fields:
* the name of the sensor which will be used in renode, internally,
* the sensor's renode C\# class,
* the peripheral to use,
* and the (simulated) physical address of the sensor.
Execute that using the following command:
```bash
python ./scripts/run_renode.py \
    --timeout 5 \
    --frequency 100 \
    --sensor 'adxl345_1,Sensors.ADXL345,i2c1,0x3d' \
    --sensor 'adxl345_2,Sensors.ADXL345,i2c1,0x2d'
```

This should run the application smoothly using a different sensor.
You should see the following output:

```
Starting Renode simulation. Press CTRL+C to exit.
*** Booting Zephyr OS build v4.2.2-1-g17081cb00713 ***
0_0,0_1,0_2,1_0,1_1,1_2
64.662598,57.920525,32.178070,19.613300,38.920143,30.952238
59.759274,22.984335,36.468479,44.436382,69.565926,38.613686
36.162022,7.661445,33.097443,46.581589,69.872383,73.856331
62.210934,41.371803,73.549873,22.064962,14.403517,14.097059
73.549873,14.097059,9.193734,15.935806,61.291561,71.711128
0.000000,14.709975,25.435999,18.081011,48.726791,28.807034
66.501343,21.145590,1.225831,3.064578,57.307610,53.630116
39.533058,34.323277,37.387852,44.742840,51.791370,32.178070
17.161636,3.064578,0.919373,64.356140,25.129539,12.258312
2.451662,47.194504,48.113876,67.420715,46.581589,35.549107
```
