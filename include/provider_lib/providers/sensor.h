#ifndef PROVIDER_LIB_PROVIDERS_SENSOR_H
#define PROVIDER_LIB_PROVIDERS_SENSOR_H

#include <provider_lib/provider.h>

struct provider_sensor_info
{
    const struct device *sensor;
};

int provider_read_sensor(struct provider *p, float *buf);

int provider_reader_register_all_sensor(struct provider_reader *pr);

#endif // PROVIDER_LIB_PROVIDERS_SENSOR_H