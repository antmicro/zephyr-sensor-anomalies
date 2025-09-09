#include <math.h>

#include <anomaly_lib/detector.h>

#include <kenning_inference_lib/core/model.h>
#include <kenning_inference_lib/core/utils.h>

#include <model_data.h>

static size_t input_N, output_N;

static int32_t detector_kenning_detect(struct detector *d, const float *buf, float *score)
{
    status_t status = STATUS_OK;

    float outputs[16]; // outputs should be 2 floats long

    status = model_load_input((uint8_t *)buf, input_N);
    if (STATUS_OK != status)
        return status;

    status = model_run();
    if (STATUS_OK != status)
        return status;

    size_t model_output_size;
    status = model_get_output(output_N, (uint8_t *)outputs, &model_output_size);
    if (STATUS_OK != status)
        return status;

    *score = exp(outputs[0]) / (exp(outputs[0]) + exp(outputs[1]));

    return 0;
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

    status = model_get_input_size(&input_N);
    if (STATUS_OK != status)
        return status;

    status = model_get_output_size(&output_N);
    if (STATUS_OK != status)
        return status;

    return 0;
}

struct detector g_detector = {
    .detect = detector_kenning_detect,
    .init = detector_kenning_init,
};
