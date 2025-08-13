#ifndef PROVIDER_LIB_PROVIDER_
#define PROVIDER_LIB_PROVIDER_

#include <stdint.h>

#include <zephyr/drivers/sensor.h>

#define MAX_PROVIDER_COUNT 10

enum provider_status
{
    PROVIDER_STATUS_OK = 0,
    PROVIDER_STATUS_ERROR = -1 << 31,
    PROVIDER_STATUS_SAMPLE_FETCH_ERR,
    PROVIDER_STATUS_CHAN_GET_ERR,
    PROVIDER_STATUS_OOB_ERR,
};

struct provider
{
    enum sensor_channel *channels;
    uint32_t channels_N;

    int (*read_fn)(struct provider *, float *);
    void *info;
};

struct provider_reader
{
    struct provider *ps[MAX_PROVIDER_COUNT];
    uint32_t ps_N;
    uint32_t dst_N;
};

struct provider_hdr_entry
{
    uint32_t provider_id;
    enum sensor_channel chan;
};

int provider_reader_register(struct provider_reader *pr, struct provider *p);

int provider_reader_read_all(struct provider_reader *pr, float *dst);

int provider_reader_hdr(struct provider_reader *pr, struct provider_hdr_entry *hdr);

#endif // PROVIDER_LIB_PROVIDER_
