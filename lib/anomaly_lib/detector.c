/*
 * Copyright (c) 2026 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "provider_lib/provider.h"
#include "provider_lib/providers/sensor.h"
#include "zephyr/arch/arch_interface.h"
#include <zephyr/kernel.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <anomaly_lib/detector.h>

struct anomaly_ctx
{
    struct detector_classifier *dc;
    struct k_work work;
    float score;
};

K_KERNEL_STACK_DEFINE(dc_thread_stack, CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_STACKSIZE);
K_KERNEL_STACK_DEFINE(dc_wq_stack, CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_STACKSIZE);
K_HEAP_DEFINE(dc_heap, CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_HEAPSIZE);

static inline uint64_t time_delta(int64_t *time)
{
    uint64_t cur_time = k_uptime_get();
    int64_t delta64 = cur_time - *time;
    if (delta64 < 0)
    {
        delta64 += UINT32_MAX;
    }
    *time = cur_time;

    return delta64;
}

static void dc_launch_callbacks(struct detector_classifier *dc, float score)
{
    for (int i = 0; i < dc->num_cbs; ++i)
    {
        (dc->cb_hdlrs[i])(dc->cb_ctxs[i], score);
    }
}

static void dc_anomaly_worker(struct k_work *work)
{
    struct anomaly_ctx *anomaly_ctx = CONTAINER_OF(work, struct anomaly_ctx, work);

    dc_launch_callbacks(anomaly_ctx->dc, anomaly_ctx->score);

    k_heap_free(&dc_heap, anomaly_ctx);
}

static void detector_classifier_thread_entry(struct detector_classifier *dc, void *p2, void *p3)
{
    UNUSED(p2);
    UNUSED(p3);
    int32_t status;
    int64_t t_elapsed, t_start;
    float score;

    while (atomic_get(&dc->deinit_started) == 0)
    {
        t_start = k_uptime_get();
        status = provider_reader_read_all(dc->pr, dc->buffer);
        if (status)
        {
            /* Some error */
            return;
        }

        status = detector_detect(dc->buffer, &score);
        if (status)
        {
            return;
        }

        if (score >= dc->threshold)
        {
            /* Is an anomaly. Launch all callbacks */
            /* struct anomaly_ctx anomaly_ctx; */
            struct anomaly_ctx *anomaly_ctx = k_heap_alloc(&dc_heap, sizeof(struct anomaly_ctx), K_NO_WAIT);
            if (anomaly_ctx)
            {
                anomaly_ctx->dc = dc;
                anomaly_ctx->score = score;
                k_work_init(&anomaly_ctx->work, dc_anomaly_worker);
                k_work_submit_to_queue(&dc->wq, &anomaly_ctx->work);
            }
            else
            {
                /* Using this thread to launch callbacks because heap is full */
                dc_launch_callbacks(dc, score);
            }
        }

        t_elapsed = time_delta(&t_start);
        int64_t to_wait = dc->process_ms - t_elapsed;
        if (to_wait > 0)
        {
            k_msleep(to_wait);
        }
        else
        {
            printk("Detection too slow: %lld\n", to_wait);
        }
    }
}

int32_t detector_classifier_init(struct detector *d, struct detector_classifier *dc, struct provider_reader *pr,
                                 int64_t processing_delay, float threshold)
{

    int32_t status = DETECTOR_STATUS_OK;

    if (!d || !dc || !pr)
    {
        return -DETECTOR_STATUS_EINVAL;
    }

    dc->detector = d;
    dc->threshold = threshold;
    dc->process_ms = processing_delay;
    dc->pr = pr;

    if (sizeof(dc->buffer) / sizeof(dc->buffer[0]) < pr->dst_N)
    {
        /* Insufficient buffer size */
        return -DETECTOR_STATUS_ERANGE;
    }

    dc->deinit_started = ATOMIC_INIT(0);

    k_work_queue_init(&dc->wq);
    k_work_queue_start(&dc->wq, dc_wq_stack, K_THREAD_STACK_SIZEOF(dc_wq_stack), 5, NULL);

    return status;
}

int32_t detector_classifier_deinit(struct detector_classifier *dc)
{
    if (!dc)
    {
        return -DETECTOR_STATUS_EINVAL;
    }

    if (atomic_get(&dc->deinit_started) > 0)
    {
        return DETECTOR_STATUS_OK;
    }

    atomic_set(&dc->deinit_started, 1);

    k_work_queue_stop(&dc->wq, K_FOREVER);
    k_thread_join(dc->tid, K_FOREVER);

    return -DETECTOR_STATUS_ERROR;
}

int32_t detector_classifier_register_cb(struct detector_classifier *dc, detector_cb_t cb, void *ctx)
{
    if (!dc || !cb)
    {
        return -DETECTOR_STATUS_EINVAL;
    }

    if (dc->num_cbs >= CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_MAX_CB_HDLRS - 1)
    {
        return -DETECTOR_STATUS_SIZE_ERR;
    }

    dc->cb_ctxs[dc->num_cbs] = ctx;
    dc->cb_hdlrs[dc->num_cbs++] = cb;

    return DETECTOR_STATUS_OK;
}

void dector_classifier_unregister_cb(struct detector_classifier *d)
{
    if (!d)
    {
        return;
    }

    /* Remove handlers */
    for (size_t i = 0; i < d->num_cbs; ++i)
    {
        d->cb_hdlrs[i] = NULL;
    }

    d->num_cbs = 0;
}

int32_t detector_classifier_start(struct detector_classifier *dc, int priority, int thread_opts)
{
    if (!dc)
    {
        return -DETECTOR_STATUS_EINVAL;
    }

    dc->tid = k_thread_create(&dc->thread_data, dc_thread_stack, K_THREAD_STACK_SIZEOF(dc_thread_stack),
                              (k_thread_entry_t)detector_classifier_thread_entry, dc, NULL, NULL, priority, thread_opts,
                              K_NO_WAIT);
    if (!dc->tid)
    {
        // Failed to spawn thread
        return -DETECTOR_STATUS_ERROR;
    }

    return DETECTOR_STATUS_OK;
}
