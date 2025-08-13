#!/usr/bin/env -S uv run --script
#
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "pyyaml",
# ]
# ///
import yaml

import sys

with open(sys.argv[1], "r") as f:
    data = yaml.safe_load(f)

template_HANDLE_X = """
#define HANDLE_{compat}(__node_id) \
    BUILD_HANDLE(__node_id, channels_{compat})
"""

template_undef_HANDLE_X = """
#undef HANDLE_{compat}
"""

template_channels_X = """
static enum sensor_channel channels_{compat}[] = {{
    {str_channels}
}};
"""

template_DT_FOREACH_STATUS_OKAY = """
DT_FOREACH_STATUS_OKAY({compat}, HANDLE_{compat});
"""

template_register_all_impl = """
int register_all_sensor_impl(struct provider_reader *pr, int (*read_fn)(struct provider*, float*)) {{
    {channel_decls}
    {foreaches}
    return 0;
}}
"""

preamble = """
#include <stdint.h>

#include <zephyr/drivers/sensor.h>

#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

#define BUILD_HANDLE(__node_id, __channels) \
    do {    \
        static struct provider_sensor_info info = { \
            .sensor = DEVICE_DT_GET(__node_id)  \
        };  \
        static struct provider p = {    \
            .channels = __channels, \
            .channels_N = sizeof(__channels) / sizeof(__channels[0]), \
            .read_fn = provider_read_sensor, \
            .info = &info \
        }; \
        provider_reader_register(pr, &p); \
    } while(0);
"""

entries = {}

for entry in data:
    compat = next(iter(entry.keys()))
    channels = next(iter(entry.values()))
    entries[compat] = channels


out_file = open(sys.argv[2], "w")

print(preamble, file=out_file)

for compat, channels in entries.items():
    print(template_HANDLE_X.format(compat=compat), file=out_file)

print(
    template_register_all_impl.format(
        channel_decls="\n".join(
            (
                template_channels_X.format(
                    compat=compat,
                    str_channels=",\n    ".join(("SENSOR_CHAN_" + x for x in channels)),
                )
                for compat, channels in entries.items()
            )
        ),
        foreaches="\n".join(
            (
                template_DT_FOREACH_STATUS_OKAY.format(compat=compat)
                for compat, _ in entries.items()
            )
        ),
    ),
    file=out_file,
)

for compat, channels in entries.items():
    print(template_undef_HANDLE_X.format(compat=compat), file=out_file)

out_file.close()
