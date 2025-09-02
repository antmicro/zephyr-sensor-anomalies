zephyr_library_named(anomaly_lib)

set(src "")

cmake_path(ABSOLUTE_PATH CONFIG_ANOMALY_LIB_EMLEARN_MODEL_PATH BASE_DIRECTORY ${APPLICATION_CONFIG_DIR})

if(${CONFIG_ANOMALY_LIB_BACKEND_EMLEARN})
    list(APPEND src "detectors/emlearn.c")
    list(APPEND src "detectors/emlearn_model_impl.c")
endif()

add_custom_command(
    OUTPUT detectors/emlearn_model_impl.c
    COMMAND ${CMAKE_CURRENT_LIST_DIR}/../../scripts/gen_emlearn.py ${CONFIG_ANOMALY_LIB_EMLEARN_MODEL_PATH} detectors/emlearn_model_impl.c
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/../../scripts/gen_emlearn.py ${CONFIG_ANOMALY_LIB_EMLEARN_MODEL_PATH}
)

zephyr_library_sources(${src})
