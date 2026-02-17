#include "zephyr/toolchain.h"
#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdio.h>

#include <anomaly_lib/detector.h>

K_KERNEL_STACK_DEFINE(wq_stack, DETECTOR_THREADS_STACKSIZE);

static inline bool is_anomaly(float threshold, float score) { return score >= threshold; }

static void drain_wq(struct detector *d)
{
    size_t p = atomic_load(&d->producer_idx);

    while (d->consumer_idx != p)
    {
        float val = d->wq_buf[d->consumer_idx];
        d->consumer_idx = (d->consumer_idx + 1) % DETECTOR_WORK_Q_RINGBUF_SIZE;
        if (is_anomaly(d->threshold, val))
        {
            for (size_t i = 0; i < d->num_cbs; ++i)
            {
                (d->cb_hdlrs[i])(d->cb_ctxs[i], val);
            }
        }
    }
}

static void detector_work_handler(struct k_work_delayable *work)
{
    struct detector *d = CONTAINER_OF(work, struct detector, dw);
    int64_t start = k_uptime_get();

    // Batched processing
    drain_wq(d);

    int64_t elapsed = k_uptime_get() - start;
    int64_t next = d->process_ms - (int32_t)elapsed;

    // We're behind. Catch up!
    if (next <= 0)
    {
        k_work_schedule_for_queue(&d->wq, &d->dw, K_NO_WAIT);
    }
    else
    {
        k_work_schedule_for_queue(&d->wq, &d->dw, K_MSEC(next));
    }
}

int32_t detector_classifier_submit_sample(struct detector *d, float sample)
{
    size_t p = atomic_load(&d->producer_idx);
    size_t next = (p + 1) % DETECTOR_WORK_Q_RINGBUF_SIZE;

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

int32_t detector_classifier_register_cb(struct detector *det, detector_cb_t cb, void *ctx)
{
    if (!det || !cb)
    {
        return -DETECTOR_STATUS_ERROR;
    }

    if (det->num_cbs >= DETECTOR_MAX_CB_HANDLERS - 1)
    {
        return -DETECTOR_STATUS_SIZE_ERR;
    }

    det->cb_ctxs[det->num_cbs] = ctx;
    det->cb_hdlrs[det->num_cbs++] = cb;

    return DETECTOR_STATUS_OK;
}

void detector_classifier_unregister_cb(struct detector *det)
{
    if (!det)
    {
        return;
    }

    // Remove handlers
    for (size_t i = 0; i < det->num_cbs; ++i)
    {
        det->cb_hdlrs[i] = NULL;
    }
}
