# Getting Started

## Installing dependencies

To get started with the library, ensure that [uv](https://docs.astral.sh/uv/) is installed first.
Also, ensure that your system has [Zephyr dependencies](https://docs.zephyrproject.org/latest/develop/getting_started/index.html#install-dependencies) installed.

Clone the repository to get started:

```bash
mkdir -p zephyr-workspace && cd zephyr-workspace
git clone https://github.com/antmicro/zephyr-sensor-anomalies
cd zephyr-sensor-anomalies
```

Initialize the Python virtual environment:

```bash
uv venv --python=3.11
source .venv/bin/activate
uv pip install west
```

Initialize the `west` build tool and the Zephyr project:

```bash
west init -l
west update
west patch apply
west zephyr-export
uv pip install -r ../zephyr/scripts/requirements.txt
# Install the toolchain for your board
west sdk install -t arm-zephyr-eabi
```

Because the example application uses Kenning, you need to install it:
```bash
uv pip install 'kenning[renode,reports,tensorflow,tflite,torch,uart,zephyr] @ git+https://github.com/antmicro/kenning'
```

## Building the anomaly detection sample

This section provides a walkthrough of building one of the examples provided in the repository.
The final output is an emulated Zephyr application running on [Renode](https://renode.io/), which detects the presence of anomalies from data obtained by sensors.

### Demo Application for reading and printing sensor data

The example consists of training an anomaly detection model for a set of two [LIS2DS12 accelerometers](https://www.st.com/en/mems-and-sensors/lis2ds12.html).
The sample app in `samples/sensors` will read values from all available sensors and print them to the Zephyr console UART.

First, build the application:
```bash
west build -p -b stm32f746g_disco samples/sensors
```

The application can be run on physical hardware and collect actual sensor readings.
For the purposes of this demonstration, the `run_renode.py` script may be used to launch a Renode simulation and feed data from a CSV file to the simulated sensors.

Download sample data:

```bash
curl https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv -o data.csv
```

Inspecting the CSV file, the first 8 columns correspond to the dataset features;
while the last column indicates whether the features in the given row is anomalous.

```csv
roll,pitch,gx,gy,gz,ax,ay,az,anomaly
-0.0000,0.0013,-0.0042,0.1317,-0.0200,-0.0039,-0.0007,-0.0744,0
-0.0001,0.0025,-0.0063,0.1134,-0.0177,0.0005,0.0001,-0.1005,0
-0.0002,0.0033,-0.0055,0.0890,-0.0151,0.0010,0.0001,-0.1006,0
-0.0002,0.0040,-0.0052,0.0669,-0.0132,0.0009,0.0001,-0.1002,0n
-0.0002,0.0041,-0.0024,0.0107,-0.0126,0.0030,0.0000,-0.1017,0
-0.0003,0.0034,-0.0043,-0.0708,-0.0156,0.0044,-0.0001,-0.1026,0
-0.0003,0.0020,-0.0051,-0.1391,-0.0165,0.0034,-0.0000,-0.1020,0
-0.0004,0.0002,-0.0052,-0.1827,-0.0155,0.0017,0.0000,-0.1010,0
-0.0004,-0.0019,-0.0054,-0.2100,-0.0139,0.0005,0.0000,-0.1001,0
```

Download Renode and set necessary environment variables:
```bash
./scripts/prepare_renode.sh
source ./scripts/activate_renode.sh
```

Then use the script to run a simulation:
```bash
python ./scripts/run_renode.py --timeout 5 --frequency 100 --data data.csv
```

This will run the simulation for 5 seconds, feeding sensor data from the `data.csv` file with the frequency of 100Hz.
One line in the CSV represents one data sample (for all sensors).
If no file is provided, random values will be used.

### Creating an anomaly detection model

Once data is gathered from the sensors, a model can be trained to detect anomalies from the sensors in real time.
In this example, a simple binary classifier is used to train the anomaly detection model.
A Kenning scenario (a YAML file with Kenning configuration) from one of the example `workflows` provided in this repository can be used.

The model can be trained with a labeled dataset that Kenning will automatically download from `https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv`.

```bash
pushd workflows/minispot
kenning train --cfg scenario.yml
```

After training the model, compile it for efficient execution using `TFLite Micro` runtime:

```bash
kenning optimize --cfg scenario.yml
```

This will generate two files: `fp32.1.tflite` and `fp32.1.tflite.json`.
Both will be saved in `build`.

To run this model, another example application may be used - `samples/anomaly`.
While `samples/sensors` simply prints the sensor outputs to UART, `samples/anomaly` will run them through the anomaly detection model.

The Zephyr Sensor Anomalies library will do the following:
* **Pre-Processing**: creating a sliding window,
* **Post-Processing**: "smoothing" the output by removing outliers and using output from the binary classifier to compute an anomaly "metric" of data.

The library does so automatically, so this step does not require any additional setup from the user.

Go back to the main project folder:
```bash
popd
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

Then, use the mentioned script and run it on random data:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 20
```

Alternatively, run the app on the dataset downloaded earlier:

```bash
python ./scripts/run_renode.py --timeout 5 --frequency 20 --data data.csv
```

The output should look as such:

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

The first 6 columns are sensor values. The rightmost column is the model's prediction (e.g., the probability of the anomaly).

The application will emit a warning if your model is too slow; the basis for this is the expected inference time provided by the `CONFIG_PROCESSING_DELAY` Kconfig option (the default value is 50 ms).

The `sample/anomaly` application can easily be used on a different model and sensor array.
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
  -DCONFIG_KENNING_MODEL_PATH="$(realpath ./workflows/minispot/build/fp32.1.tflite)"
```

In previous examples, `run_renode.py` script was used, which generated a Renode .repl file (file describing the simulated platform) in the `/tmp` directory, for the board based on Zephyr DTS file, using `gen_repl.py` script.
Kenning requires the `.repl` file to be located in the `build directory`, so call `gen_repl.py` directly:
```bash
python ./scripts/gen_repl.py --repl build/stm32f746g_disco.repl
```

Please note, that the filename must be the same as name of the board being used.

A Kenning scenario will be used.
It can be found in one of the example workflows:

```bash
pushd workflows/minispot
kenning test report \
  --report-path reports/minispot.md \
  --to-html \
  --cfg scenario.yml \
  --measurements results.json
```

In `report/report/report.html`, you can find an HTML version of the report that contains:

* confusion matrix,
* inference quality metrics,
* detection rate (number of anomalies detected),
* false alarm rate (number of false positives),
* detection delay depending on detection threshold.

To run this evaluation on a different board, create an overlay file telling Kenning which UART port to use by adding the `kcomms` alias.
An example can be found in `samples/anomaly/boards/stm32f746g_disco.overlay`.
You should use this file to define any additional sensors that are not part of the board by default.
In such case, please remember you need to modify the `gen_repl.py` script in order to add these sensors to the `.repl`, and update the sensor information in `scenario.yml`.

## Zephyr sensor anomalies structure

The `zephyr-sensor-anomalies` library contains the following directory structure:
* `docs` with markdown files for the documentation,
* `include` with library headers, divided into two sub-libraries:
  * `anomaly_lib` which provides headers for detecting anomalies based on a trained anomaly detection model,
  * `provider_lib` which provides headers for a lightweight abstraction to register multiple heterogenous data sources and read all provider data as a single continuous float array.
* `lib` containing the library for `anomaly_lib` and `provider_lib`,
* `samples` with samples of applications that use the library,
* `scripts` with helper scripts to aid in development,
* `workflows` containing an end-to-end build system showcasing how the sample application, library, and Kenning work together,
* `zephyr` containing Zephyr-specific settings.
