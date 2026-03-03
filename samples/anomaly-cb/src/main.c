/*
 * Copyright (c) 2026 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "kenning_inference_lib/core/protocol.h"
#include "zephyr/toolchain.h"
#include <zephyr/kernel.h>

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include <anomaly_lib/detector.h>
#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

#include <kenning_inference_lib/core/kenning_protocol.h>
#include <kenning_inference_lib/core/loaders.h>

#ifdef CONFIG_KENNING_PROTOCOL_INTEGRATION

struct msg_loader *msg_loader_picker(message_type_t type)
{
    static uint8_t payload_buffer[128];
    static struct msg_loader ldr = MSG_LOADER_BUF(payload_buffer, sizeof(payload_buffer));
    return &ldr;
}

/**
 * Initializes the Kenning protocol and performs the handshake.
 */
int initialize_protocol()
{
    int status;

    status = protocol_init();
    if (status)
    {
        printk("Protocol init failed: %d\n", status);
        return status;
    }

    protocol_event_t recv_event;

    status = protocol_listen(&recv_event, msg_loader_picker);

    if (status)
    {
        printk("Protocol listen failed: %d\n", status);
        return status;
    }

    if (recv_event.message_type != MESSAGE_TYPE_PING || !recv_event.flags.general_purpose_flags.success)
    {
        printk("Invalid message received");
        return -1;
    }

    protocol_event_t send_event = {0};
    send_event.flags.general_purpose_flags.success = 1;
    send_event.message_type = MESSAGE_TYPE_PING;
    send_event.payload.size = 0;

    status = protocol_transmit(&send_event);

    if (status)
    {
        printk("Failed to respond: %d\n", status);
        return status;
    }

    return 0;
}
#else
int initialize_protocol() { return -1; }
#endif // CONFIG_KENNING_PROTOCOL_INTEGRATION

#ifdef CONFIG_KENNING_PROTOCOL_INTEGRATION

/**
 * Sends results via Kenning protocol.
 */
int send_score(float score)
{
    static float output_buffer[128];
    output_buffer[0] = score;
    output_buffer[1] = 1 - score;

    protocol_event_t send_event = {0};
    send_event.flags.general_purpose_flags.success = 1;
    send_event.message_type = MESSAGE_TYPE_OUTPUT;
    send_event.payload.size = 2 * sizeof(float);
    send_event.payload.raw_bytes = (uint8_t *)output_buffer;

    protocol_transmit(&send_event);
}
#else
int send_score(float score) { return 0; }
#endif // CONFIG_KENNING_PROTOCOL_INTEGRATION

/**
 * Basic logging callback.
 */
static void log_score_cb(void *ctx, float score) { printf("Anomaly detected with probability %.5f\n", (double)score); }

/**
 * Callback to send score using kenning protocol.
 */
static void send_score_cb(void *ctx, float score)
{
    int protocol_initialized = *((int *)ctx);
    if (protocol_initialized)
    {
        send_score(score);
    }
}

#if CONFIG_MAX_ANOMALY_COUNT > 0
/**
 * Callback to check if CONFIG_MAX_ANOMALY_COUNT has been reached.
 * If so, stop detecting more anomalies and shut down the application.
 * Used for automated tests.
 */
static void anomaly_ctr_cb(void *ctx, float _score)
{
    ARG_UNUSED(_score);
    atomic_inc((atomic_t *)ctx);
}
#endif // CONFIG_MAX_ANOMALY_COUNT

int main()
{
    int status;
#if CONFIG_MAX_ANOMALY_COUNT > 0
    atomic_t anomaly_ctr = ATOMIC_INIT(0);
#endif // CONFIG_MAX_ANOMALY_COUNT

    static struct provider_reader pr = {
        .dst_N = 0,
        .ps_N = 0,
    };

    static struct provider_hdr_entry hdr[128];

    status = provider_reader_register_all_sensor(&pr);
    if (status)
    {
        printk("Sensor registration failed: %d\n", status);
        return -1;
    }

    status = provider_reader_hdr(&pr, hdr);
    if (status)
    {
        printk("Header generation failed: %d\n", status);
        return -1;
    }

    int protocol_initialized = !initialize_protocol();

    status = detector_init();
    if (status)
    {
        printk("Detector init failed: %d\n", status);
        return -1;
    }

    struct detector_classifier dc = {0};

    status = detector_classifier_init(&g_detector, &dc, &pr, 100, 0.5);
    if (status)
    {
        printk("Classifier init failed: %d\n", status);
        return -1;
    }

    status = detector_classifier_register_cb(&dc, log_score_cb, NULL);
    if (status)
    {
        printk("Failed to register log_score_cb: %d\n", status);
        return -1;
    }

    status = detector_classifier_register_cb(&dc, send_score_cb, (void *)&protocol_initialized);
    if (status)
    {
        printk("Failed to register send_score_cb: %d\n", status);
        return -1;
    }

#if CONFIG_MAX_ANOMALY_COUNT > 0
    status = detector_classifier_register_cb(&dc, anomaly_ctr_cb, (void *)&anomaly_ctr);
    if (status)
    {
        printk("Failed to register anomaly_ctr_cb: %d\n", status);
    }
#endif // CONFIG_MAX_ANOMALY_COUNT

    status = detector_classifier_start(&dc, 6, DETECTOR_DEFAULT_OPTS);
    if (status)
    {
        printk("Failed to start classifier: %d\n", status);
        return -1;
    }

#if CONFIG_MAX_ANOMALY_COUNT > 0
    while (atomic_get(&anomaly_ctr) < CONFIG_MAX_ANOMALY_COUNT)
    {
        k_msleep(10);
    }

    detector_classifier_deinit(&dc);
    printk("Successfully shut down classifier\n");
#else
    while (1)
    {
        k_msleep(1000);
    }
#endif // CONFIG_MAX_ANOMALY_COUNT
}
