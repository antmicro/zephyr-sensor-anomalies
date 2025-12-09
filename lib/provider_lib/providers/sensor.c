/*
 * Copyright (c) 2025 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

int provider_read_sensor(struct provider *p, float *buf)
{
    int rc;
    struct sensor_value val[3]; // this prevents overflows, when a vector channel is specified
    struct provider_sensor_info *info = p->info;
    rc = sensor_sample_fetch(info->sensor);
    if (rc)
    {
        return -PROVIDER_STATUS_SAMPLE_FETCH_ERR;
    }

    for (uint32_t i = 0; i < p->channels_N; i++)
    {
        rc = sensor_channel_get(info->sensor, p->channels[i], val);
        if (rc)
        {
            return -PROVIDER_STATUS_CHAN_GET_ERR;
        }

        buf[i] = sensor_value_to_float(val);
    }

    return PROVIDER_STATUS_OK;
}

int provider_reader_register_all_sensor(struct provider_reader *pr) { return register_all_sensor_impl(pr); }
