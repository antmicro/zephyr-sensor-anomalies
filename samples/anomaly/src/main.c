#include <zephyr/kernel.h>

#include <stdio.h>

#include <anomaly_lib/detector.h>
#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

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

    float score;

    while (1)
    {
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

            status = detector_detect(buffer, &score);
            if (status)
            {
                printk("Detector failed: %d\n", status);
                return -1;
            }

            printf(",%f", score);
            puts("");
            break;
        }
        k_msleep(500);
    }
}
