# Real-time Anomaly Detection

This tutorial assumes that you have a fully working setup of this library.
For more information, check the page on [reading sensor data](reading-sensor-data).

Another purpose of this library is allowing the detection of anomalies from provider readings.
The process begins by training a basic supervised anomaly detection model using [Kenning](https://kenning.ai).

The provider library can be used in two ways:
* implementing code to gather sensor data and detecting in your own loop (in which case, you will need to handle detection and threshold manually),
* using the callback system where detection, buffering, and sliding windows are implictly defined, with some degree of flexibility.

This tutorial follows an example from `workflows/minispot`, showcasing the first method, as well as showing how to perform the same task using the callback method.

## Training an anomaly detection model

This section provides a walkthrough on embedding a trained anomaly detection model using the provider library, starting with training a simple model up to running a simulation with Renode.
For more information on how implement, train, and optimize more complex models, check the [Kenning documentation](https://antmicro.github.io/kenning/).

In a workspace directory (e.g., `anomaly_app/`), create the Kenning scenario `scenario.yml`:

```yaml
platform:
  type: ZephyrPlatform
  parameters:
    name: stm32f746g_disco
    simulated: True
    zephyr_build_path: ../build
    sensors:
      - i2c1.lis2ds12_1
      - i2c1.lis2ds_12_2
    sensors_frequency: 99.5
    runtime_init_log_msg: "*** Booting Zephyr OS build"
    disable_profiler: true
model_wrapper:
  type: PyTorchGenericClassification
  parameters:
    model_path: ./net.pt
    model_source: ./model.py
    training_batch_size: 512
    num_epochs: 10
dataset:
  type: TabularDataset
  parameters:
    dataset_root: build/data
    dataset_path: https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv
    cols_x: ["roll", "pitch", "gz", "ax", "ay", "az"]
    col_y: "anomaly"
    window_size: 8
    shuffle_data: False
    download_dataset: True
inference_loop:
  type: AnomalyDetectionInferenceLoop
  parameters:
    inference_limit: 512
optimizers:
- type: TFLiteCompiler
  parameters:
    compiled_model_path: fp32.1.tflite
    inference_input_type: float32
    inference_output_type: float32
    target: default
```

The `model.py` will contain a basic Pytorch ResNet.
For now, copy the model from `workflows/minispot/model.py` to the `anomaly_app` directory.

The model is then ready to be trained and optimized:

```
pushd anomaly_app
kenning train --cfg scenario.yml
kenning optimize --cfg scenario.yml
popd anomaly_app
```

You should see the optimized model `fp32.1.tflite` in the same directory.

## Basic inference

After training the model, the next step is setting up an application that will use the model for inference.
For the purposes of this tutorial, the anomaly detection model used will be a supervised model with a sigmoid activation function and threshold of `0.5`.

Create the `main.c` as follows:

```c
#include <zephyr/kernel.h>

#include <stdio.h>
#include <assert.h>

#include <anomaly_lib/detector.h>
#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

#include <kenning_inference_lib/core/kenning_protocol.h>
#include <kenning_inference_lib/core/loaders.h>

#define READER_DELAY 100

int main(void)
{
    /* Initialize the provider to read from the sensor */
    static struct provider_reader pr = {0};
    static float buffer[128] = {0};
    static struct provider_hdr_entry hdr[128] = {0};

    provider_reader_register_all_sensor(&pr);
    provider_reader_hdr(&pr, hdr);

    /* Fill in the headers */
    for (int i = 0; i < pr.dst_N; ++i) {
        if (i > 0) {
            printk(",");
        }
        printk(",%u_%u", hdr[i].provider_id, hdr[i].chan);
    }
    printk("\n");

    /* Receive sensor data */
    while (1)
    {
        provider_reader_read_all(&pr, buffer);

        /* Print out the results */
        for (int i = 0; i < pr.dst_N; ++i) {
            if (i > 0) {
                printk(",");
            }
            printk(",%f", (double) buffer[i]);
        }
        printk("\n");

        /* Let us put some delay here */
        k_msleep(READER_DELAY);
    }

    return 0;
}
```

The DTS overlay to be used should be written in `anomaly_app/boards/stm32f46g_disco.overlay` with the following contents:

```
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

By now, your directory structure should be similar to the following:
```
anomaly_app/
    boards/
        stm32f746g_disco.overlay
    src/
        main.c
    prj.conf
    CMakeLists.txt
    scenario.yml
    fp32.1.tflite
    model.py
    ... <other files>
```

To build the application, use:

```bash
west build -p -b stm32f746g_disco anomaly_app
```

An example time-series dataset is minispot which used [OpenQuadruped](https://github.com/OpenQuadruped/spot_mini_mini) to record data.
The timestep is considered anomalous when the quadruped stumbles during the simulation.
A prepared dataset can be downloaded as a time-series CSV file:

```csv
curl https://dl.antmicro.com/kenning/datasets/anomaly_detection/minispot.csv -o data.csv
```

Renode is required to be able to run the simulation.
The next commands download and activate Renode:

```bash
./scripts/prepare_renode.sh
source ./scripts/activate_renode.sh
```

Now that the sensor setup and dataset preparation are complete, the anomaly detection logic can be integrated in the main processing loop:

```c
/* In the defines portion of the application */
#define ANOMALY_THRESHOLD 0.5


/* More code here */

detector_init();

while (1)
{
    /* Read sensor data here */

    float score;
    detector_detect(buffer, &score);

    if (score > ANOMALY_THRESHOLD)
    {
        printf("Detected anomaly with confidence: %f\n", (double) score);
    }

    /* Delay here */
    k_msleep(READER_DELAY);
}
```

However, more libraries have to be enabled for the inference.
In `prj.conf`, add the following:

```
# KENNING_ML_RUNTIME_TFLITE requires C++
CONFIG_CPP=y
CONFIG_STD_CPP17=y

# Enable the library for anomaly detection
CONFIG_ANOMALY_LIB=y
# Enable the zephyr library for loading models and running inference
CONFIG_KENNING_INFERENCE_LIB=y
# kenning_inference_lib will use tflite
CONFIG_KENNING_ML_RUNTIME_TFLITE=y
# Size in kb of the buffer for the tensor arena allocator
CONFIG_KENNING_TFLITE_BUFFER_SIZE=128
# Use kenning as the backend for anomaly_lib.
CONFIG_ANOMALY_LIB_BACKEND_KENNING=y
# The input to the model is larger than the default input count.
# Let us give a larger number to accomodate the model's input size
CONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT=256
```

The following command will build the application alongside the compiled tflite model:

```bash
west build -p -b stm32f746g_disco workspace -- \
    -DCONFIG_KENNING_MODEL_PATH=\"$(realpath ./workspace/fp32.1.tflite)\"
```

Assuming that `LIS2DS12` sensors are used, run the application using Renode with the following command:

```bash
python ./scripts/run_renode.py \
    --timeout 5 \
    --frequency 100 \
    --data data.csv
```

You should see an output similar to the one below.

```
Starting Renode simulation. Press CTRL+C to exit.
*** Booting Zephyr OS build v4.2.2-1-g17081cb00713 ***
0_0,0_1,0_2,1_0,1_1,1_2
0.000000,0.001196,-0.004785,0.129212,-0.022731,-0.003589,0.628548
Detected anomaly with confidence: 0.628548
0.000000,0.000000,-0.008374,0.110069,-0.017946,0.000000,0.884736
Detected anomaly with confidence: 0.884736
0.000000,0.000000,-0.008374,0.110069,-0.017946,0.000000,0.949333
Detected anomaly with confidence: 0.949333
0.000000,0.000000,-0.008374,0.086141,-0.017946,0.000000,0.942706
Detected anomaly with confidence: 0.942706
0.000000,0.001196,-0.004785,0.009571,-0.013160,0.000000,0.948326
Detected anomaly with confidence: 0.948326
0.000000,0.000000,-0.004785,-0.070588,-0.017946,0.001196,0.958815
Detected anomaly with confidence: 0.958815
0.000000,0.000000,-0.004785,-0.185443,-0.017946,0.001196,0.902647
```

The anomaly detection application is complete.

## Using Callbacks
In some applications, the existing control flow and processing logic may make direct integration of the anomaly detection loop impractical.
For example, you may prefer to focus on core application functionality rather than managing anomaly detection explicitly within the application logic.

To address these scenarios, you may use the callback mechanism to run anomaly detection asynchronously.
This approach allows the application to react to detected anomalies through user-defined callbacks without the primary control flow.
To address these scenarios, the callback mechanism enables anomaly detection to run asynchronously.
This approach allows the application to react to detected anomalies through user-defined callbacks while keeping the main application logic unchanged.

The following example demonstrates a callback-based anomaly detection workflow.
Here, the main loop represents the primary application logic, while anomaly detection is executed asynchronously through the callback mechanism.

This approach allows anomaly detection to operate independently of the application's main logic.

```c
#include <zephyr/kernel.h>

#include <stdio.h>
#include <assert.h>

#include <anomaly_lib/detector.h>
#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

#include <kenning_inference_lib/core/kenning_protocol.h>
#include <kenning_inference_lib/core/loaders.h>

#define READER_DELAY 100
#define CALLBACK_PRIORITY 6

static void log_score_cb(void *ctx, float score);

int main(void)
{
    /* Initialize the provider to read from the sensor */
    static struct provider_reader pr = {0};
    static float buffer[128] = {0};
    static struct provider_hdr_entry hdr[128] = {0};

    provider_reader_register_all_sensor(&pr);
    provider_reader_hdr(&pr, hdr);

    detector_init();

    /* Context for the callback */
    struct detector_classifier dc = {0};

    detector_classifier_init(
        &g_detector,              /* Global detector object (initialized by detector_init()) */
        &dc,                      /* The detector context */
        &pr,                      /* The provider reader */
        DETECTOR_SMOOTHING_NONE,  /* Smoothing method */
        100,                      /* Detector processing delay in milliseconds */
        0.5                       /* Threshold */
    );

    /* Registering a callback that activates when an anomaly is detected. */
    detector_classifier_register_cb(
        &dc,           /* The detector context */
        log_score_cb,  /* Name of the callback */
        NULL           /* Opaque context specific to the callback */
    );

    /* Starting the detection loop */
    detector_classifier_start(&dc, CALLBACK_PRIORITY, DETECTOR_DEFAULT_OPTS);

    while (1)
    {
        /* Complex application-specific code */
        k_msleep(1000);
    }

    detector_classifier_deinit(&dc);

    return 0;
}

/* Function that gets called when an anomaly is detected */
static void log_score_cb(void *ctx, float score)
{
    (void) ctx;
    /* Logging the anomaly */
    printk("Anomaly detected with confidence: %.4f\n", (double) score);
}
```

You may pass up to `ANOMALY_LIB_DETECTION_CALLBACKS_MAX_CB_HDLRS` for every `detector_classifier` object.
By default, you can pass up to `8` callbacks that will be called sequentially everytime an anomaly is detected.
Increasing the `MAIN_STACK_SIZE` is necessary; otherwise, the kernel will quickly run out of memory.
See [here](https://docs.zephyrproject.org/latest/develop/optimizations/footprint.html) for further details.
Furthermore, `CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS` enables the callback mechanism:

```
CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS=y
CONFIG_MAIN_STACK_SIZE=4096
```

Rebuild the application and run again with Renode:

```bash
west build -p -b stm32f746g_disco workspace -- \
    -DCONFIG_KENNING_MODEL_PATH=\"(realpath ./workspace/fp32.1.tflite)\"

python ./scripts/run_renode.py --timeout 5 --frequency 20 --data data.csv
```

You should see something similar to:

```
Starting Renode simulation. Press CTRL+C to exit.
*** Booting Zephyr OS build v4.2.2-1-g17081cb00713 ***
Anomaly detected with confidence: 0.6285
Anomaly detected with confidence: 0.9971
Anomaly detected with confidence: 0.5771
Anomaly detected with confidence: 0.5232
Anomaly detected with confidence: 0.9755
```

## Evaluating the trained model with Kenning

The evaluation of the accuracy of the model within the application is supported with the use of the Kenning Protocol.
After training and optimizing the model, one can generate a report with Kenning which includes the following:
* confusion matrix,
* inference quality metrics,
* detection rate (number of anomalies detected),
* false alarm rate,
* detection delay depending on detection threshold.

The Kenning scenario has the following YAML block:
```yaml
platform:
  type: ZephyrPlatform
  parameters:
    name: stm32f746g_disco
    simulated: True
    zephyr_build_path: <path/to/workspace>/build
    sensors:
      - i2c1.lis2ds12_1
      - i2c1.lis2ds_12_2
    sensors_frequency: 99.5
    runtime_init_log_msg: "*** Booting Zephyr OS build"
    disable_profiler: true
```

This dictates how the model get tested using Renode.
Once you have a trained model, you can begin the test with the following command:

```bash
cd workspace
kenning test report \
    --report-path reports/anomaly.md \
    --to-html \
    --cfg scenario.yml \
    --measurements results.json
```

Opening the report at `reports/anomaly/report.html` should show a complete report of the model performance.
You can also see an example real-time anomaly detection evaluation report [here](sample-kenning-evaluation-report).

Check out the [kenning docs](https://antmicro.github.io/kenning/cmd-usage.html#generating-performance-reports) for more information on generating reports with kenning.
