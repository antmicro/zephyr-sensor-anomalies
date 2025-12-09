/*
 * Copyright (c) 2025 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>

#include <stdio.h>

#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

int main()
{
    static struct provider_reader pr = {
        .dst_N = 0,
        .ps_N = 0,
    };

    static float buffer[CONFIG_CSV_READER_MAX_ROW_SIZE];
    static struct provider_hdr_entry hdr[CONFIG_CSV_READER_MAX_ROW_SIZE];

    provider_reader_register_all_sensor(&pr);
    provider_reader_hdr(&pr, hdr);

    if (sizeof(buffer) / sizeof(buffer[0]) < pr.dst_N)
    {
        printk("Insufficient buffer size: %u < %u", sizeof(buffer) / sizeof(buffer[0]), pr.dst_N);
        return -1;
    }

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

    uint32_t sample_count = 0;

    while (1)
    {
        provider_reader_read_all(&pr, buffer);

        uint32_t i = 0;
        while (1)
        {
            printf("%lf", (double)buffer[i]);
            if (++i < pr.dst_N)
            {
                putchar(',');
                continue;
            }
            puts("");
            break;
        }
        k_msleep(CONFIG_CSV_READER_DELAY);

        if (CONFIG_CSV_READER_SAMPLE_COUNT > 0 && ++sample_count >= CONFIG_CSV_READER_SAMPLE_COUNT)
        {
            break;
        }
    }
}
