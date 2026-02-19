/*
 * Copyright (c) 2026 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ANOMALY_LIB_DETECTOR_H
#define ANOMALY_LIB_DETECTOR_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/kernel.h>

enum detector_status
{
    DETECTOR_STATUS_OK = 0,
    DETECTOR_STATUS_ERROR,
    DETECTOR_STATUS_SIZE_ERR,
};

/**
 * Callback type when classifier threshold is met.
 *
 * @param ctx    Opaque context passed by caller.
 * @param score  Logit output of the classifier.
 */
typedef void (*detector_cb_t)(void *ctx, float score);

struct detector
{
    /* Detector-specific implementation of `detector_detect` */
    int32_t (*detect)(struct detector *, const float *, float *);
    int32_t (*init)(struct detector *);
    int32_t (*get_info)(struct detector *, void **);

#ifdef CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS
    /* Anomaly detection callback handler struct members */
    /** work queue */
    int8_t is_wq_init;
    int8_t is_wq_start;

    struct k_work_q wq;
    struct k_work_delayable dw;

    /* Consumer and producer indices for the ring buffer */
    _Atomic size_t producer_idx, consumer_idx;

    /** Number of registered callbacks */
    size_t num_cbs;
    /** Struct containing the registered callbacks */
    detector_cb_t cb_hdlrs[CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_MAX_CB_HDLRS];
    void *cb_ctxs[CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_MAX_CB_HDLRS];

    /** Thresholds on when the callbacks will be called */
    int64_t process_ms;
    float threshold;
    float wq_buf[CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_WORK_Q_RINGBUF_SIZE];
#endif /* CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS */
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

static inline int32_t detector_init()
{
    if (!g_detector.init)
    {
        return 0;
    }
    return g_detector.init(&g_detector);
}

static inline int32_t detector_get_info(void *info)
{
    if (!g_detector.get_info)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    return g_detector.get_info(&g_detector, info);
}

#ifdef CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS
/**
 * Initialize the callback-based classifier. After initialization, caller needs to add up to 8
 * callbacks manually.
 *
 * @param d            Uninitialized classifier detector object. Need to call `detector_init()` first.
 * @param threshold    Threshold for the logit to be considered an anomaly or not.
 * @param process_ms   The target cadence of the classifier. Scheduling callbacks will take less than this
 *                     in the event that the work handlers take a longer time than `process_ms`.
 * @param smoothing    The smoothing method to use. Currently unused but probably moving averages or
 *                     kalman filter.
 */
int32_t detector_classifier_init(struct detector *d, float threshold, int64_t process_ms, int smoothing);

/**
 * Start the timer and work queue for the classifier with scheduling priority of `prio`
 *
 * @param d      Initialized classifier detector object. The object shouldn't have been started yet.
 * @param prio   Scheduling priority
 */
int32_t detector_classifier_start(struct detector *d, int prio);

/**
 * Shut down and deinitialize the callback-handling system.
 *
 * @param d  Initialized detector object
 */
int32_t detector_classifier_deinit(struct detector *d);

/**
 * Submit a score/sample logit to the classifier.
 *
 * @param d         Detector object
 * @param sample    Logit detected
 */
int32_t detector_classifier_submit_sample(struct detector *d, float sample);

/**
 * Submit batched scores/logits to the classifier. The reduces the function call overhead,
 * number of atomic updates, and better memory throughput.
 *
 * @param d         Detector object
 * @param samples   Array of logits/scores
 * @param n_samples The number of samples in `samples` array.
 */
int32_t detector_classifier_submit_samples_batch(struct detector *d, float *samples, size_t n_samples);

/**
 * Register a callback that is run when an anomaly is detected.
 *
 * @param d      The detector object.
 * @param cb     Callback function
 * @param ctx    Opaque context passed to this specific callback.
 */
int32_t detector_classifier_register_cb(struct detector *d, detector_cb_t cb, void *ctx);

/**
 * Unregister all callbacks
 *
 * @param d      The detector object
 */
void detector_classifier_unregister_cb(struct detector *d);
#endif /* CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS */

#endif // ANOMALY_LIB_DETECTOR_H
