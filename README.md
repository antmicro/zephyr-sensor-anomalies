# Zephyr Sensor Anomalies

Copyright (c) 2025-2026 [Antmicro](https://www.antmicro.com)

Zephyr Sensor Anomalies is a [Zephyr RTOS](https://www.zephyrproject.org/) library that allows to track anomalies in a provided set of sensors using Machine Learning models ran with [Kenning Zephyr Runtime](https://github.com/antmicro/kenning-zephyr-runtime) or [emlearn](https://github.com/emlearn/emlearn).

It provides tools for:

* reading data from sensors and using it to generate datasets,
* setting up analysis of sensor readings using neural networks, kNN, decision trees, etc.,
* setting up callbacks on detected anomalies,
* evaluating anomaly detection using the [Kenning](https://kenning.ai) framework.

## Installing dependencies

Install [Zephyr dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies):

```bash
apt update
apt install --no-install-recommends -y git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 libicu-dev xxd
```

Install `west` with `uv` (or `pip` if `uv` is not available):

```bash
uv venv --python=3.11
source .venv/bin/activate
uv pip install west
```

Initialize Zephyr:

```bash
west init -l .
west update
west patch apply
west zephyr-export
uv pip install -r ../zephyr/scripts/requirements.txt
west sdk install -t arm-zephyr-eabi
```

To use Kenning, install it:

```bash
uv pip install 'kenning[torch,tflite,reports,uart,renode,tensorflow] @ git+https://github.com/antmicro/kenning'
```

## Example workflow

As an example, we will go through the steps of training an anomaly detection model for a set of two [LIS2DS12 accelerometers](https://www.st.com/en/mems-and-sensors/lis2ds12.html).

#### Demo application for reading and printing sensor data

A sample app provided in `samples/sensors` will read values from all available sensors and print them to the Zephyr console UART.

First, build the application:

```bash
west build -p -b stm32f746g_disco samples/sensors
```

The application can be run on physical hardware and collect actual sensor readings.
However, for the purposes of this demonstration, we will use the `run_renode.py` script to launch a [Renode](https://renode.io/) simulation and feed data from a CSV file to the simulated sensors.

For that, download sample data:

```bash
curl https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv -o data.csv
```

For this demonstration, Renode will be required to run the simulation.
Download Renode and export environmental variables pointing to its location:

```bash
./scripts/prepare_renode.sh
source ./scripts/activate_renode.sh
```

Please note that the second script must be used any time you use a new bash.

Then, use the script to run a simulation:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 100 --data data.csv
```

This will run the simulation for 5 seconds, feeding sensor data from the `data.csv` file with the frequency of 100Hz.
One line in the CSV file represents one data sample (for all sensors).
If no file is provided, random values will be used.

With some modifications, the script may also be used to test other applications with different sensor arrays.
To do that, modify functions `prepare_repl` in `gen_repl.py` and `get_sensors` in `run_renode.py`.

#### Creating an anomaly detection model

An example Kenning PyTorch model will be trained, defined in `workflows/minispot/model.py`.
It will be a simple binary classifier.
A Kenning scenario (a YAML file with Kenning configuration) from one of the example `workflows` provided with this repository can be used.

The model will be trained with a labeled dataset, which is automatically downloaded by Kenning from `https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv`.

```bash
cd workflows/minispot
kenning train --cfg scenario.yml
```

After training the model, compile it for efficient execution using `TFLite Micro` runtime:

```bash
kenning optimize --cfg scenario.yml
```

This will generate two files: `fp32.1.tflite` and `fp32.1.tflite.json`.
Both files will be saved in `build`.

To run this model, another example application may be used - `samples/anomaly`.
The previously used `samples/sensors` prints the sensor outputs to UART; `samples/anomaly` will run these outputs through the given model.

The Zephyr Sensor Anomalies library will take care of data pre-processing (creating a sliding window) and post-processing ("smoothing" the output by removing outliers and using output from the binary classifier to compute an "anomaly metric").

Go back to the main project folder:

```bash
cd ../../
```

And build the application with the model:

```bash
west build -p -b stm32f746g_disco samples/anomaly -- \
  -DCONFIG_KENNING_TFLITE_BUFFER_SIZE=100 \
  -DCONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT=1024 \
  -DCONFIG_KENNING_PROTOCOL_INTEGRATION=n  \
  -DEXTRA_CONF_FILE=kenning.conf \
    -DCONFIG_KENNING_MODEL_PATH=\"$(realpath ./workflows/minispot/build/fp32.1.tflite)\"
```

Then, either use the previously mentioned script to run it on random data:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 20
```

Or run the app on the dataset downloaded earlier:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 20 --data data.csv
```

The output should look something like this:

```bash skip
0.775274,0.105284,0.373280,0.637687,0.612562,0.991824,0.999648
0.857826,0.588634,0.695115,0.809970,0.736989,0.967896,0.989194
0.714257,0.872183,0.009571,0.843470,0.387637,0.071784,0.999842
0.866201,0.814756,0.120837,0.952343,0.656829,0.719043,0.998614
0.330209,0.293120,0.789631,0.211764,0.090927,0.996610,0.934414
0.229710,0.454636,0.053838,0.971486,0.853041,0.230907,0.248174
0.674775,0.010767,0.943968,0.551545,0.585045,0.202193,0.996867
0.909272,0.440279,0.384048,0.100498,0.167497,0.967896,0.998475
0.369691,0.632901,0.144765,0.924825,0.708275,0.843470,0.912978
0.066999,0.503689,0.966700,0.398404,0.451047,0.267996,0.934081
0.044267,0.234496,0.492921,0.173479,0.652044,0.546760,0.622759
0.622133,0.579063,0.191425,0.943968,0.378065,0.494117,0.789022
0.068195,0.373280,0.407976,0.805184,0.717846,0.173479,0.303264
0.722632,0.154337,0.551545,0.617348,0.722632,0.933200,0.001524
0.494117,0.427118,0.044267,0.770488,0.665204,0.240478,0.003188
0.775274,0.090927,0.971486,0.135194,0.541974,0.028713,0.985690
0.618544,0.513260,0.369691,0.436690,0.082552,0.311066,0.999548
0.747757,0.449850,0.369691,0.202193,0.867398,0.851844,0.991345
```

The first 6 columns are sensor values; the rightmost column shows the model's prediction (0 - anomaly, 1 - no anomaly).

The application will emit a warning if your model is too slow; the basis for this is the expected inference time provided by the `CONFIG_PROCESSING_DELAY` Kconfig option (the default value is 50 ms).

The `sample/anomaly` application can easily be used on a different model or sensor array.
If you wish to do that, the code of the application does not need to be modified in any way (since it automatically detects available sensors); however, be sure the sensor you are using is described in `lib/provider_lib/sensor_map.yaml`.
All you need to do is change the structure of the model in the `model.py` file.

#### Evaluating the model in Kenning

The `samples/anomaly` application supports Kenning Protocol.
Because of that, it can be used for not only training and optimization, but also for evaluating the accuracy of the model and generating a report with Kenning.

For that, first build the application with Kenning Protocol integration enabled.
To achieve that, the value of `DCONFIG_KENNING_PROTOCOL_INTEGRATION` is changed:

```bash
west build -p -b stm32f746g_disco samples/anomaly -- \
  -DCONFIG_KENNING_TFLITE_BUFFER_SIZE=100 \
  -DCONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT=1024 \
  -DCONFIG_KENNING_PROTOCOL_INTEGRATION=y  \
  -DEXTRA_CONF_FILE=kenning.conf \
    -DCONFIG_KENNING_MODEL_PATH=\"$(realpath ./workflows/minispot/build/fp32.1.tflite)\"
```

In previous examples, `run_renode.py` script was used, which generated a Renode .repl file (file describing the simulated platform) in the `/tmp` directory, for the board based on Zephyr DTS file, using `gen_repl.py` script.
Kenning requires the `.repl` file to be located in the `build directory`, so call `gen_repl.py` directly:

```bash
python ./scripts/gen_repl.py --repl build/stm32f746g_disco.repl
```

Please note that the filename must be the same as the name of the board being used.

A Kenning scenario will be used.
It can be found in one of the example workflows:
```bash
cd workflows/minispot
kenning test report \
  --report-path report/report.md \
  --to-html \
  --cfg scenario.yml \
  --measurements results.json
```

In `report/report/report.html`, you can find an HTML version of the report that contains:

- confusion matrix,
- inference quality metrics,
- detection rate (number of anomalies detected),
- false alarm rate (number of false positives),
- detection delay depending on detection threshold.

To run this evaluation on a different board, create an overlay file telling Kenning which UART port to use by adding the `kcomms` alias.
An example can be found in `samples/anomaly/boards/stm32f746g_disco.overlay`.
You should use this file to define any additional sensors that are not part of the board by default.
In such case, please remember you need to modify the `gen_repl.py` script in order to add these sensors to the `.repl`, and update the sensor information in `scenario.yml`.


## Other workflows

The example workflow presented is automated using a Makefile at `workflows/minispot`.
To run it automatically, go to that folder and run `make eval` (this will train the model and generate a report).

You can explore other example workflows in the `workflows` directory by running (and analyzing) the Makefiles.
