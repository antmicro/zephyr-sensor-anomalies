# Zephyr Sensor Anomalies

Copyright (c) 2025 [Antmicro](https://www.antmicro.com)

Zephyr Sensor Anomalies is a [Zephyr RTOS](https://www.zephyrproject.org/) library that allows to track anomalies in provided set of sensors using Machine Learning models ran with [Kenning Zephyr Runtime](https://github.com/antmicro/kenning-zephyr-runtime) or [emlearn](https://github.com/emlearn/emlearn).

It provides tools for:

* Reading data from sensors and using it to generate datasets
* Setting up analysis of sensor readings using neural networks, kNN, decision trees, ...
* Setting up callbacks on detected anomalies
* Evaluating anomaly detection using [Kenning](https://kenning.ai) framework

## Installing dependencies

Install [Zephyr dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies):

```bash
apt update
apt install --no-install-recommends -y git cmake ninja-build gperf \
  ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk \
  xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1 libicu-dev xxd
```

then, install `west` with `uv` (or `pip` if `uv` is not available):

```bash
uv venv --python=3.11
source .venv/bin/activate
uv pip install west
```

and initialize Zephyr:

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

As an example, let's go through the steps of training an anomaly detection model for a set of 2 [LIS2DS12 accelerometers](https://www.st.com/en/mems-and-sensors/lis2ds12.html).

#### Demo application for reading and printing sensor data

A sample app provided in `samples/sensors` will read values from all available sensors and print them to the Zephyr console UART.

First, build the application:

```bash
west build -p -b stm32f746g_disco samples/sensors
```

Application could be ran on real hardware and collect actual sensor readings.
However for the purposes of this demonstration let's will use `run_renode.py` script to launch a [Renode](https://renode.io) simulation and feed data from a CSV file to the simulated sensors.

For that, let's download some sample data:

```bash
curl https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv -o data.csv
```

For simulation purposes, download Renode and export some environmental variables pointing to its location:

```bash
./scripts/prepare_renode.sh
source ./scripts/activate_renode.sh
```

Please note, that the second script must be used any time a new bash is used.

Finally, use the script to run a simulation:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 100 --data data.csv
```

This will run the simulation for 5 seconds, feeding sensor data from `data.csv` file with the frequency of 100Hz.
One line in the CSV represents 1 data sample (for all sensors).
If no file is provided, random values will be used.

With some modifications the script may also be used to test other applications, with different sensor arrays.
To do that, modify functions `prepare_repl` in `gen_repl.py` and `get_sensors` in `run_renode.py`.

#### Creating an anomaly detection model

Let's train an example Kenning PyTorch model, defined in `workflows/minispot/model.py` - a simple binary classifier.
A Kenning scenario (a YAML file with Kenning configuration), from one of the example `workflows` provided with this repository, can be used.

Model will be trained with a labeled dataset, that Kenning will automatically download from `https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv`.

```bash
cd workflows/minispot
kenning train --cfg scenario.yml
```

After training the model, compile it for efficient execution using `TFLite Micro` runtime:

```bash
kenning optimize --cfg scenario.yml
```

This will generate 2 files: `fp32.1.tflite` and `fp32.1.tflite.json`, both will be saved in `build`.

To run this model, another example application may be used - `samples/anomaly`.
While `samples/sensors` simply prints the sensor outputs to UART, this one will run them through the given model.

The Zephyr Sensor Anomalies library will take care of pre-processing (creating a sliding window) and post-processing ("smoothing" the output by removing outliers and using output from the binary classifier to compute an "anomaly metric") of data.

Let's go back to the main project folder:

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

Then, use the aforementioned script run it on random data:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 20
```

Or run the app on the dataset downloaded earlier:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 20 --data data.csv
```

Output should look something like this:

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

First 6 columns are sensor values, while the rightmost column is the model's prediction (0 - anomaly, 1 - no anomaly).

The application will also emit a warning, if your model is too slow, based on the expected inference time provided (in `ms`) with `CONFIG_PROCESSING_DELAY` Kconfig option (default value is 50).

The `sample/anomaly` application can easily be used on a different model and sensor array.
To do that, there is no need to change the code of the application itself (since it automatically detects available sensors) - just make sure the sensor you're using is described in `lib/provider_lib/sensor_map.yaml`.
All you need to do is change structure of the model in the `model.py` file.

#### Evaluating the model in Kenning

Aside from training and optimization, `samples/anomaly` app may be used together with Kenning, to evaluate accuracy of the model and generate a report.
That is because the application supports Kenning Protocol.

For that, first build the application with Kenning Protocol integration enabled (notice how `-DCONFIG_KENNING_PROTOCOL_INTEGRATION=y` is set this time):

```bash
west build -p -b stm32f746g_disco samples/anomaly -- \
  -DCONFIG_KENNING_TFLITE_BUFFER_SIZE=100 \
  -DCONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT=1024 \
  -DCONFIG_KENNING_PROTOCOL_INTEGRATION=y  \
  -DEXTRA_CONF_FILE=kenning.conf \
    -DCONFIG_KENNING_MODEL_PATH=\"$(realpath ./workflows/minispot/build/fp32.1.tflite)\"
```

In previous examples, `run_renode.py` script was used, which generated a Renode .repl file (file describing the simulated platform) in the `/tmp` directory, for the board based on Zephyr DTS file, using `gen_repl.py` script.
Kenning needs the `.repl` file to be located in the `build directory`, so let's call `gen_repl.py` directly:

```bash
python ./scripts/gen_repl.py --repl build/stm32f746g_disco.repl
```

Please note, that the filename must be the same as name of the board being used.

Let's use once again a Kenning scenario from one of example workflows:

```bash
cd workflows/minispot
kenning test report \
  --report-path report/report.md \
  --to-html \
  --cfg scenario.yml \
  --measurements results.json
```

At `report/report/report.html` an HTML version of the report can be found, containing:

- confusion matrix
- inference quality metrics
- detection rate (how many anomalies were detected) and false alarm rate (how many of anomalies detected were not, in fact, anomalies) depending on detection threshold
- detection delay depending on detection threshold

In order to run this evaluation on a different board, create an overlay file, telling Kenning which UART port to use, by adding `kcomms` alias.
Example at `samples/anomaly/boards/stm32f746g_disco.overlay`.
In this file also define any additional sensors, that are not by default part of the board - in such a case please remember to also modify `gen_repl.py` script to add these sensors to the `.repl` and update sensor info in `scenario.yml`.


## Other workflows

The example workflow presented above (creating and testing a simple binary classifier) is automated using a Makefile at `workflows/minispot`.
To run it automatically go to that folder and run `make eval` (this will train the model and generate a report).

Feel free to explore other example workflows in the `workflows` directory, by running (and analyzing) the Makefiles.
