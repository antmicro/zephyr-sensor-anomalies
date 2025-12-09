/*
 * Copyright (c) 2025 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ANOMALY_LIB_DETECTORS_KENNING_H_
#define ANOMALY_LIB_DETECTORS_KENNING_H_

#include <anomaly_lib/detector.h>

struct kenning_detect_info
{
    float probs[CONFIG_ANOMALY_LIB_KENNING_MAX_OUTPUT_COUNT];
};

#endif // ANOMALY_LIB_DETECTORS_KENNING_H_