/*
 * Copyright (c) 2025 Antmicro <www.antmicro.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PROVIDER_LIB_PROVIDER_
#define PROVIDER_LIB_PROVIDER_

#include <stdint.h>

#include <zephyr/drivers/sensor.h>

enum provider_status
{
    PROVIDER_STATUS_OK = 0,
    PROVIDER_STATUS_ERROR,
    PROVIDER_STATUS_SAMPLE_FETCH_ERR,
    PROVIDER_STATUS_CHAN_GET_ERR,
    PROVIDER_STATUS_OOB_ERR,
};

struct provider
{
    enum sensor_channel *channels;
    /* Count of `channels` */
    uint32_t channels_N;
    /* Read function returning `channels_N` floats */
    int (*read_fn)(struct provider *, float *);
    /* Opaque provider data */
    void *info;
};

struct provider_reader
{
    struct provider *ps[CONFIG_MAX_PROVIDER_COUNT];
    /* Number of providers */
    uint32_t ps_N;
    /* Total number channels */
    uint32_t dst_N;
};

struct provider_hdr_entry
{
    uint32_t provider_id;
    enum sensor_channel chan;
};

/**
 * Registers a single provider in the provider reader and increments `dst_N`.
 *
 * Usually called from helpers like `provider_reader_register_all_sensor`.
 */
int provider_reader_register(struct provider_reader *pr, struct provider *p);

/**
 * Reads from all providers in the provider reader using `read_fn` and collates results into a float buffer.
 *
 * `dst` size must be greater or equal than `dst_N`.
 */
int provider_reader_read_all(struct provider_reader *pr, float *dst);

/**
 * Generates a header containing information on provider id and channel of every element in the float buffer
 * returned by `provider_reader_read_all`.
 *
 * `hdr` size must be greater or equal than `dst_N`. Header doesn't change between reads
 * and should be generated only after provider registration.
 */
int provider_reader_hdr(struct provider_reader *pr, struct provider_hdr_entry *hdr);

#endif // PROVIDER_LIB_PROVIDER_
