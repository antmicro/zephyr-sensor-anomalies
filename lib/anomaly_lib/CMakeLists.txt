# Copyright (c) 2025 Antmicro <www.antmicro.com>
#
# SPDX-License-Identifier: Apache-2.0


zephyr_library_named(anomaly_lib)

set(src "")

if(${CONFIG_ANOMALY_LIB_BACKEND_EMLEARN})
    cmake_path(ABSOLUTE_PATH CONFIG_ANOMALY_LIB_EMLEARN_MODEL_PATH BASE_DIRECTORY ${APPLICATION_CONFIG_DIR})

    list(APPEND src "detectors/emlearn.c")
    list(APPEND src "detectors/emlearn_model_impl.c")

    add_custom_command(
        OUTPUT detectors/emlearn_model_impl.c
        COMMAND ${CMAKE_CURRENT_LIST_DIR}/../../scripts/gen_emlearn.py ${CONFIG_ANOMALY_LIB_EMLEARN_MODEL_PATH} detectors/emlearn_model_impl.c ${CONFIG_ANOMALY_LIB_EMLEARN_THRESHOLD}
        DEPENDS ${CMAKE_CURRENT_LIST_DIR}/../../scripts/gen_emlearn.py ${CONFIG_ANOMALY_LIB_EMLEARN_MODEL_PATH}
    )
endif()

if(${CONFIG_ANOMALY_LIB_BACKEND_KENNING})
    add_dependencies(anomaly_lib lib_generate_model_data_h)
    include_directories(${APPLICATION_BINARY_DIR}/modules/kenning-zephyr-runtime/lib/kenning_inference_lib/)

    list(APPEND src "detectors/kenning.c")

endif()

if(${CONFIG_ANOMALY_LIB_BACKEND_NONE})
    message(FATAL_ERROR "No anomaly backend selected. Enable one of 'CONFIG_ANOMALY_LIB_BACKEND_*' or disable 'CONFIG_ANOMALY_LIB'.")
endif()

if(${CONFIG_ANOMALY_LIB_DETECTION_CALLBACKS})
  list(APPEND src "detector.c")
endif()
zephyr_library_sources(${src})
