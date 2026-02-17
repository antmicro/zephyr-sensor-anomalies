/*
 * Copyright (c) 2025 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <string.h>

#include <anomaly_lib/detector.h>
#include <anomaly_lib/detectors/kenning.h>

#include <kenning_inference_lib/core/model.h>
#include <kenning_inference_lib/core/utils.h>

#include <model_data.h>

#include <zephyr/kernel.h>

/* Model specific input and output sizes in bytes. */
static size_t gp_input_N, gp_output_N;

/* Kenning detector specific extra detection info. */
static struct kenning_detect_info gp_info;

/**
 * Maintains a sliding window in `outputbuf` from the samples passed in `inputbuf`.
 */
static int32_t detector_kenning_sliding_window(struct detector *d, const float *inputbuf, float *outputbuf)
{
    size_t row_N, col_N;
    size_t num_input_dim = model_spec.num_input_dim[0];
    row_N = model_spec.input_shape[0][num_input_dim - 2];
    col_N = model_spec.input_shape[0][num_input_dim - 1];

    for (uint32_t i = 0; i < row_N - 1; i++)
    {
        memcpy(outputbuf + i * col_N, outputbuf + (i + 1) * col_N, col_N * sizeof(*outputbuf));
    }

    memcpy(outputbuf + (row_N - 1) * col_N, inputbuf, col_N * sizeof(*outputbuf));

    return DETECTOR_STATUS_OK;
}

/**
 * Runs the inference and computes the outlier score.
 */
static int32_t detector_kenning_detect_run(struct detector *d, const float *buf, float *score)
{
    status_t status = STATUS_OK;

    float outputs[CONFIG_ANOMALY_LIB_KENNING_MAX_OUTPUT_COUNT]; // outputs should be 2 floats long

    status = model_load_input((uint8_t *)buf, gp_input_N);
    if (STATUS_OK != status)
        return status;

    status = model_run();
    if (STATUS_OK != status)
        return status;

    size_t model_output_size;
    status = model_get_output(gp_output_N, (uint8_t *)outputs, &model_output_size);
    if (STATUS_OK != status)
        return status;

    *score = exp(outputs[0]) / (exp(outputs[0]) + exp(outputs[1]));

    memcpy(gp_info.probs, outputs, gp_output_N);

    return DETECTOR_STATUS_OK;
}

/**
 * Wrapper for `detector_kenning_detect_run` that supports the sliding window.
 */
static int32_t detector_kenning_detect(struct detector *d, const float *buf, float *score)
{
    static float sliding_window_buffer[CONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT];

    detector_kenning_sliding_window(d, buf, sliding_window_buffer);

    return detector_kenning_detect_run(d, sliding_window_buffer, score);
}

static int32_t detector_kenning_init(struct detector *d)
{
    status_t status = STATUS_OK;

    status = model_init();
    if (STATUS_OK != status)
        return status;

    status = model_load_struct((uint8_t *)&model_spec, sizeof(model_spec_t));
    if (STATUS_OK != status)
        return status;

    status = model_load_weights(model_data, model_data_len);
    if (STATUS_OK != status)
        return status;

    status = model_get_input_size(&gp_input_N);
    if (STATUS_OK != status)
        return status;

    status = model_get_output_size(&gp_output_N);
    if (STATUS_OK != status)
        return status;

    if (gp_input_N > CONFIG_ANOMALY_LIB_KENNING_MAX_INPUT_COUNT)
    {
        return -DETECTOR_STATUS_SIZE_ERR;
    }

    if (gp_output_N > CONFIG_ANOMALY_LIB_KENNING_MAX_OUTPUT_COUNT)
    {
        return -DETECTOR_STATUS_SIZE_ERR;
    }

#ifdef CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS
    d->num_cbs = 0;
    d->cb_hdlrs[0] = NULL;
    d->wq_buf[0] = 0.0;
#endif

    return DETECTOR_STATUS_OK;
}

static int32_t detector_kenning_get_info(struct detector *d, void **info)
{
    *(struct kenning_detect_info **)info = &gp_info;
    return DETECTOR_STATUS_OK;
}

struct detector g_detector = {
    .detect = detector_kenning_detect,
    .init = detector_kenning_init,
    .get_info = detector_kenning_get_info,
};
