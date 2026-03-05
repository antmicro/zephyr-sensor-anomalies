/*
 * Copyright (c) 2025-2026 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ANOMALY_LIB_DETECTOR_H
#define ANOMALY_LIB_DETECTOR_H

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>

#include <zephyr/kernel.h>

#include <provider_lib/provider.h>

/**
 * No smoothing used for anomaly detection.
 */
#define DETECTOR_SMOOTHING_NONE NULL
#define DETECTOR_SMOOTHING_EXP_SMOOTHING detector_smoothing_exp_smoothing

enum detector_status
{
    DETECTOR_STATUS_OK = 0,
    DETECTOR_STATUS_ERROR,
    DETECTOR_STATUS_SIZE_ERR,
    DETECTOR_STATUS_EINVAL,
    DETECTOR_STATUS_ERANGE,
};

struct detector_classifier;

struct detector
{
    /* Detector-specific implementation of `detector_detect` */
    int32_t (*detect)(struct detector *, const float *, float *);
    int32_t (*init)(struct detector *);
    int32_t (*get_info)(struct detector *, void **);
};

#ifdef CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS
/**
 * Callback type when classifier threshold is met.
 *
 * @param ctx    Opaque context passed by caller.
 * @param score  Logit output of the classifier.
 */
typedef void (*detector_cb_t)(void *ctx, float score);

/**
 * Type for the smoothing function.
 */
typedef float (*detector_smoothing_cb_t)(struct detector_classifier *dc, float score, void *ctx);

struct detector_smoothing_opts_exp_mav
{
    /** Value between 0 and 1. */
    float smoothing_factor;
};

float detector_smoothing_exp_smoothing(struct detector_classifier *dc, float score, void *ctx);

/**
 * Callback handler manager for anomaly detection.
 */
struct detector_classifier
{
    /** Detector object for this specific classifier */
    struct detector *detector;
    struct provider_reader *pr;
    atomic_t deinit_started;
    struct k_work_q wq;
    float buffer[128];
    struct provider_hdr_entry hdr[128];

    float threshold;
    int64_t process_ms;

    struct k_thread thread_data;
    k_tid_t tid;

    /** Number of registered callbacks */
    size_t num_cbs;
    /** Struct containing the registered callbacks */
    detector_cb_t cb_hdlrs[CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_MAX_CB_HDLRS];
    void *cb_ctxs[CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_MAX_CB_HDLRS];

    /** Smoothing function */
    detector_smoothing_cb_t smoothing;
    /** Current value of the moving average */
    float smoothed_moving_average;
    /** Indicator whether smoothing started. Used to prevent initializing
        `smoothing_score` to some absurd number such as 0 or NaN. */
    bool smoothing_started;

    /** Options for the smoothing average. Needs to be set after
        calling `detector_classifier_init` */
    union
    {
        struct detector_smoothing_opts_exp_mav exp_moving_avg_opts;
    };
};
#endif // CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS

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

#define DETECTOR_DEFAULT_OPTS K_FP_REGS

#ifdef CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS
/**
 * Initialize the detector classifier struct.
 *
 * @param d                  Initialized detector
 * @param dc                 Uninitialized detector classifier
 * @param pr                 Pointer to the initialized provider_reader object
 * @param smoothing          Function used for smoothing scores. Possible values are:
 *                           - DETECTOR_SMOOTHING_NONE - no smoothing.
 *                           - DETECTOR_SMOOTHING_EXP_SMOOTHING - exponential smoothing.
 * @param processing_delay   Cadence for obtaining new values from the detector.
 * @param threshold          Probability threshold in (0, 1) to determine whether
 *                           a score is anomaly or not.
 */
int32_t detector_classifier_init(struct detector *d, struct detector_classifier *dc, struct provider_reader *pr,
                                 detector_smoothing_cb_t smoothing, int64_t processing_delay, float threshold);

/**
 * Start the callback handler in the background.
 *
 * @param dc               Initialized classifier object
 * @param priority         Priority given to thread.
 * @param thread_opts      Pass `DETECTOR_DEFAULT_OPTS` for the default settings.
 *                         Otherwise, this contains standard Zephyr thread options.
 */
int32_t detector_classifier_start(struct detector_classifier *dc, int priority, int thread_opts);

/**
 * Stop the callback handler and cleanup
 *
 * @param dc    classifier object
 */
int32_t detector_classifier_deinit(struct detector_classifier *dc);

/**
 * Register a callback that is run when an anomaly is detected.
 *
 * @param d      The detector object.
 * @param cb     Callback function
 * @param ctx    Opaque context passed to this specific callback.
 */
int32_t detector_classifier_register_cb(struct detector_classifier *dc, detector_cb_t cb, void *ctx);

/**
 * Unregister all callbacks
 *
 * @param d      The detector object
 */
void detector_classifier_unregister_cb(struct detector_classifier *dc);

#endif // CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS

#endif // ANOMALY_LIB_DETECTOR_H
