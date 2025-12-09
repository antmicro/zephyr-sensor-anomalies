# Copyright (c) 2025 Antmicro <www.antmicro.com>
#
# SPDX-License-Identifier: Apache-2.0


zephyr_library_named(provider_lib)
zephyr_library_sources(
    provider.c
    sensor_map.c
    sensor_map.yaml
    providers/sensor.c
)

add_custom_command(
    OUTPUT sensor_map.c
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/../../scripts/gen_sensor_map.py ${CMAKE_CURRENT_LIST_DIR}/sensor_map.yaml sensor_map.c
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/../../scripts/gen_sensor_map.py ${CMAKE_CURRENT_LIST_DIR}/sensor_map.yaml
)
