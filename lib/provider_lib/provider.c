#include <stdint.h>

#include <zephyr/drivers/sensor.h>

#include <provider_lib/provider.h>
#include <provider_lib/sensor_map.h>

int provider_reader_register(struct provider_reader *pr, struct provider *p)
{
    if (pr->ps_N == CONFIG_MAX_PROVIDER_COUNT)
    {
        return PROVIDER_STATUS_OOB_ERR;
    }

    pr->ps[pr->ps_N++] = p;
    pr->dst_N += p->channels_N;
    return PROVIDER_STATUS_OK;
}

int provider_reader_read_all(struct provider_reader *pr, float *dst)
{
    int rc;
    uint32_t idx = 0;

    for (uint32_t i = 0; i < pr->ps_N; i++)
    {
        struct provider *p = pr->ps[i];
        rc = p->read_fn(p, dst + idx);
        if (rc)
        {
            return rc;
        }
        idx += p->channels_N;
    }

    return PROVIDER_STATUS_OK;
}

int provider_reader_hdr(struct provider_reader *pr, struct provider_hdr_entry *hdr)
{
    uint32_t write_idx = 0;
    for (uint32_t p_idx = 0; p_idx < pr->ps_N; p_idx++)
    {
        struct provider *p = pr->ps[p_idx];
        for (uint32_t chan_idx = 0; chan_idx < p->channels_N; chan_idx++)
        {
            hdr[write_idx++] = (struct provider_hdr_entry){
                .provider_id = p_idx,
                .chan = p->channels[chan_idx],
            };
        }
    }
    return PROVIDER_STATUS_OK;
}
