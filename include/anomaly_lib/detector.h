#ifndef ANOMALY_LIB_DETECTOR_H
#define ANOMALY_LIB_DETECTOR_H

#include <stdint.h>

enum detector_status
{
    DETECTOR_STATUS_OK = 0,
    DETECTOR_STATUS_ERROR,
};

struct detector
{
    /* Detector-specific implementation of `detector_detect` */
    int32_t (*detect)(struct detector *, const float *, float *);
};

extern struct detector g_detector;

/**
 * Calculates anomaly score of provided data
 *
 * `buffer` size depends on the detector. `score` is a single float.
 */
static inline int32_t detector_detect(const float *buffer, float *score)
{
    return g_detector.detect(&g_detector, buffer, score);
}

#endif // ANOMALY_LIB_DETECTOR_H
