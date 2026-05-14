# Sample real-time anomaly detection evaluation report

This section contains a sample report, with real-time anomaly detection evaluation metrics, generated during CI.

The CI is set up as follows:

* *Environment*: Github Actions
* *Task*: Anomaly Detection on minispot dataset
* *ML training framework*: [PyTorch](https://pytorch.org/), ran with [Kenning](https://antmicro.github.io/kenning/)'s generic CNN model wrapper
* *ML execution framework*: [`TFLite Micro`](https://github.com/tensorflow/tflite-micro), running under `kenning-inference-lib` from [Kenning Zephyr Runtime](https://github.com/antmicro/kenning-zephyr-runtime),
* *Platform*: `stm32f746g_disco` board with two I2C accelerometers


```{include} generated/sample-real-time-evaluation-report/report.md
```
