/*
 * Copyright (c) 2025 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include <anomaly_lib/detector.h>

int32_t detector_emlearn_detect_impl(const float *buffer, float *score);

static int32_t detector_emlearn_detect(struct detector *d, const float *buffer, float *score)
{
    return detector_emlearn_detect_impl(buffer, score);
}

struct detector g_detector = {
    .detect = detector_emlearn_detect,
    .init = NULL,
};
