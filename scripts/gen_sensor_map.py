#!/usr/bin/env -S uv run --script
#
# Copyright (c) 2025 Antmicro <www.antmicro.com>
#
# SPDX-License-Identifier: Apache-2.0
#
# /// script
# requires-python = ">=3.12"
# dependencies = [
#     "pyyaml",
#     "jinja2",
# ]
# ///
import yaml

import sys

import jinja2

from functools import reduce
import operator

template_str = """
#include <stdint.h>

#include <zephyr/drivers/sensor.h>

#include <provider_lib/provider.h>
#include <provider_lib/providers/sensor.h>

#define BUILD_HANDLE(__node_id, __channels) \\
    do {    \\
        static struct provider_sensor_info info = { \\
            .sensor = DEVICE_DT_GET(__node_id)  \\
        };  \\
        static struct provider p = {    \\
            .channels = __channels, \\
            .channels_N = sizeof(__channels) / sizeof(__channels[0]), \\
            .read_fn = provider_read_sensor, \\
            .info = &info \\
        }; \\
        provider_reader_register(pr, &p); \\
    } while(0);

{% for compat in sensor_map.keys() %}
#define HANDLE_{{ compat }}(__node_id) \\
    BUILD_HANDLE(__node_id, channels_{{ compat }})
{% endfor %}

int register_all_sensor_impl(struct provider_reader *pr, int (*read_fn)(struct provider*, float*)) {
    {% for compat, channels in sensor_map.items() %}
    static enum sensor_channel channels_{{ compat }}[] = {
        {% for channel in channels %}
        SENSOR_CHAN_{{ channel }},
        {% endfor %}
    };
    {% endfor %}

    {% for compat in sensor_map.keys() %}
    DT_FOREACH_STATUS_OKAY({{ compat }}, HANDLE_{{ compat }});
    {% endfor %}
    return 0;
}

{% for compat in sensor_map.keys() %}
#undef HANDLE_{{ compat }}
{% endfor %}
"""

with open(sys.argv[1], "r") as f:
    data = yaml.safe_load(f)

data = reduce(operator.ior, data, {})

env = jinja2.Environment()
template = env.from_string(template_str)
rendered = template.render(sensor_map=data)


with open(sys.argv[2], "w") as f:
    f.write(rendered)
