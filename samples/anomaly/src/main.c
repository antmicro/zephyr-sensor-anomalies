/*
 * Copyright (c) 2025 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <stdio.h>

#include <anomaly_lib/detector.h>
#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

#include <kenning_inference_lib/core/kenning_protocol.h>
#include <kenning_inference_lib/core/loaders.h>

/**
 * 32-bit equivalent of `k_uptime_delta`.
 */
static inline uint32_t time_delta(int32_t *time)
{
    uint32_t cur_time = k_uptime_get_32();
    int64_t delta64 = (int64_t)cur_time - *time;
    if (delta64 < 0)
    {
        delta64 += UINT32_MAX;
    }
    *time = cur_time;

    return (uint32_t)delta64;
}

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

int main()
{
    int status;
    static struct provider_reader pr = {
        .dst_N = 0,
        .ps_N = 0,
    };

    static float buffer[128];
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

    if (sizeof(buffer) / sizeof(buffer[0]) < pr.dst_N)
    {
        printk("Insufficient buffer size: %u < %u", sizeof(buffer) / sizeof(buffer[0]), pr.dst_N);
        return -1;
    }

    int protocol_initialized = !initialize_protocol();

    uint32_t i = 0;
    while (1)
    {
        printf("%d_%d", hdr[i].provider_id, hdr[i].chan);
        if (++i < pr.dst_N)
        {
            putchar(',');
            continue;
        }
        puts("");
        break;
    }

    status = detector_init();
    if (status)
    {
        printk("Detector init failed: %d\n", status);
        return -1;
    }

    uint32_t sample_count = 0;

    float score;

    uint32_t tstart, telapsed;

    while (1)
    {
        tstart = k_uptime_get_32();
        status = provider_reader_read_all(&pr, buffer);

        if (status)
        {
            printk("Sensor read failed: %d\n", status);
            return -1;
        }

        uint32_t i = 0;
        while (1)
        {
            printf("%f", buffer[i]);
            if (++i < pr.dst_N)
            {
                putchar(',');
                continue;
            }
            break;
        }

        status = detector_detect(buffer, &score);

        if (status)
        {
            printk("Detector failed: %d\n", status);
            return -1;
        }

        printf(",%f", score);
        puts("");

        if (protocol_initialized)
        {
            send_score(score);
        }

        telapsed = time_delta(&tstart);

        int32_t to_wait = CONFIG_PROCESSING_DELAY - telapsed;
        if (to_wait > 0)
        {
            k_msleep(to_wait);
        }
        else
        {
            printf("Detection too slow: %d\n", (int32_t)telapsed);
        }
    }
}
