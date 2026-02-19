/*
 * Copyright (c) 2026 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <anomaly_lib/detector.h>

K_KERNEL_STACK_DEFINE(wq_stack, CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_STACKSIZE);

static inline bool is_anomaly(float threshold, float score) { return score >= threshold; }

static void drain_wq(struct detector *d)
{

    /* It's a critical bug if the detector is NULL or started for some reason. Should never happen */
    assert(d || !d->is_wq_start);

    size_t p_idx = atomic_load(&d->producer_idx);
    size_t c_idx = atomic_load(&d->consumer_idx);

    while (c_idx != p_idx)
    {
        // Do not keep c_idx the same too long in case the handler takes too long.
        // The producer might miss the new value.
        float val = d->wq_buf[c_idx];
        c_idx = (c_idx + 1) % CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_WORK_Q_RINGBUF_SIZE;
        atomic_store(&d->consumer_idx, c_idx);

        if (is_anomaly(d->threshold, val))
        {
            for (size_t i = 0; i < d->num_cbs; ++i)
            {
                (d->cb_hdlrs[i])(d->cb_ctxs[i], val);
            }
        }

        // Refresh p_idx value in case it has changed.
        p_idx = atomic_load(&d->producer_idx);
    }
}

static void detector_work_handler(struct k_work_delayable *work)
{
    struct detector *d = CONTAINER_OF(work, struct detector, dw);

    /* It's a critical bug if the container is NULL. Should never happen */
    assert(d);

    int64_t start = k_uptime_get();

    /* Batched processing */
    drain_wq(d);

    int64_t elapsed = k_uptime_get() - start;
    int64_t next = d->process_ms - (int32_t)elapsed;

    /* We're behind. Catch up! */
    if (next <= 0)
    {
        k_work_schedule_for_queue(&d->wq, &d->dw, K_NO_WAIT);
    }
    else
    {
        k_work_schedule_for_queue(&d->wq, &d->dw, K_MSEC(next));
    }
}

int32_t detector_classifier_submit_samples_batch(struct detector *d, float *samples, size_t n_samples)
{
    if (!d || !d->is_wq_start || n_samples == 0)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    int32_t status = DETECTOR_STATUS_OK;

    size_t p_idx = atomic_load(&d->producer_idx);
    size_t c_idx = atomic_load(&d->consumer_idx);

    const size_t RING_SIZE = CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_WORK_Q_RINGBUF_SIZE;
    size_t used = p_idx - c_idx;
    size_t free_space = RING_SIZE - used;
    if (free_space < n_samples)
    {
        /* Not enough space for the new values. Reject */
        return -DETECTOR_STATUS_ERROR;
    }

    size_t contiguous = RING_SIZE - (p_idx % RING_SIZE);
    size_t first = (n_samples <= contiguous) ? n_samples : contiguous;
    size_t remaining = n_samples - first;

    memcpy(d->wq_buf + (p_idx % RING_SIZE), samples, first * sizeof *d->wq_buf);

    if (remaining > 0)
    {
        memcpy(d->wq_buf, samples + first, remaining * sizeof *d->wq_buf);
    }

    p_idx = (p_idx + n_samples) % RING_SIZE;
    atomic_store(&d->producer_idx, p_idx);

    return status;
}

int32_t detector_classifier_submit_sample(struct detector *d, float sample)
{
    if (!d || !d->is_wq_start)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    size_t p = atomic_load(&d->producer_idx);
    size_t next = (p + 1) % CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_WORK_Q_RINGBUF_SIZE;

    /* Ring buffer riched its tail. There's no more space. */
    if (next == d->consumer_idx)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    d->wq_buf[p] = sample;
    atomic_store(&d->producer_idx, next);
    return DETECTOR_STATUS_OK;
}

int32_t detector_classifier_init(struct detector *d, float threshold, int64_t process_ms, int smoothing)
{
    ARG_UNUSED(smoothing);
    if (!d || d->is_wq_init)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    if (process_ms < 1)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    if (threshold <= 0.0f || threshold >= 1.0f)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    atomic_store(&d->producer_idx, 0);
    d->consumer_idx = 0;

    k_work_init_delayable(&d->dw, (k_work_handler_t)detector_work_handler);

    d->threshold = threshold;
    d->process_ms = process_ms;

    d->is_wq_init = 1;
    d->is_wq_start = 0;

    return 0;
}

int32_t detector_classifier_deinit(struct detector *d)
{
    if (!d || !d->is_wq_init)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    k_work_cancel_delayable_sync(&d->dw, (struct k_work_sync *)&d->wq);
    int32_t status = k_work_queue_stop(&d->wq, K_FOREVER);

    if (status < 0)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    return DETECTOR_STATUS_OK;
}

int32_t detector_classifier_start(struct detector *d, int prio)
{
    int status = 0;
    if (!d || !d->is_wq_init || d->is_wq_start)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    k_work_queue_start(&d->wq, wq_stack, K_KERNEL_STACK_SIZEOF(wq_stack), prio, NULL);
    /* Schedule the first job. Do not wait. We maintain an independent
       timer from the caller */
    status = k_work_schedule_for_queue(&d->wq, &d->dw, K_NO_WAIT);
    if (status < 0)
    {
        k_work_queue_stop(&d->wq, K_NO_WAIT);
        return -DETECTOR_STATUS_ERROR;
    }
    d->is_wq_start = 1;

    return DETECTOR_STATUS_OK;
}

int32_t detector_classifier_register_cb(struct detector *d, detector_cb_t cb, void *ctx)
{
    if (!d || !cb)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    if (d->num_cbs >= CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS_MAX_CB_HDLRS - 1)
    {
        return -DETECTOR_STATUS_SIZE_ERR;
    }

    d->cb_ctxs[d->num_cbs] = ctx;
    d->cb_hdlrs[d->num_cbs++] = cb;

    return DETECTOR_STATUS_OK;
}

void dector_classifier_unregister_cb(struct detector *d)
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
