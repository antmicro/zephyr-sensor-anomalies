#include <math.h>
#include <string.h>

#include <anomaly_lib/detector.h>
#include <anomaly_lib/detectors/kenning.h>

#include <kenning_inference_lib/core/model.h>
#include <kenning_inference_lib/core/utils.h>

#include <model_data.h>

#include <zephyr/kernel.h>

static size_t gp_input_N, gp_output_N;

static struct kenning_detect_info gp_info;

static int32_t detector_kenning_sliding_window(struct detector *d, const float *inputbuf, float *outputbuf)
{
    size_t row_N, col_N;
    row_N = model_spec.input_shape[0][0];
    col_N = model_spec.input_shape[0][1];

    for (uint32_t i = 0; i < row_N - 1; i++)
    {
        memcpy(outputbuf + i * col_N, outputbuf + (i + 1) * col_N, col_N * sizeof(*outputbuf));
    }

    memcpy(outputbuf + (row_N - 1) * col_N, inputbuf, col_N * sizeof(*outputbuf));

    return DETECTOR_STATUS_OK;
}

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
