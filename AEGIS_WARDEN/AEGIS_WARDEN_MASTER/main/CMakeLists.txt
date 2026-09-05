file(GLOB_RECURSE EI_CPP
    "${CMAKE_CURRENT_LIST_DIR}/../components/edge_impulse/edge-impulse-sdk/classifier/*.cpp"
)

file(GLOB_RECURSE EI_DSP_CPP
    "${CMAKE_CURRENT_LIST_DIR}/../components/edge_impulse/edge-impulse-sdk/dsp/*.cpp"
)

# Collect ALL needed TensorFlow Lite sources (Micro + kernels + utilities)
file(GLOB_RECURSE EI_TFLITE_CPP
    "${CMAKE_CURRENT_LIST_DIR}/../components/edge_impulse/edge-impulse-sdk/tensorflow/lite/micro/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../components/edge_impulse/edge-impulse-sdk/tensorflow/lite/kernels/*.cc"
    "${CMAKE_CURRENT_LIST_DIR}/../components/edge_impulse/edge-impulse-sdk/tensorflow/lite/core/*.cc"
)

# Remove test / example files
list(FILTER EI_TFLITE_CPP EXCLUDE REGEX "/testing/")
list(FILTER EI_TFLITE_CPP EXCLUDE REGEX "/examples/")
list(FILTER EI_TFLITE_CPP EXCLUDE REGEX "/test/")
list(FILTER EI_TFLITE_CPP EXCLUDE REGEX "/benchmarks/")
list(FILTER EI_TFLITE_CPP EXCLUDE REGEX "/tools/")

file(GLOB_RECURSE EI_PORT_CPP
    "${CMAKE_CURRENT_LIST_DIR}/../components/edge_impulse/edge-impulse-sdk/porting/espressif/*.cpp"
)

set(EI_SOURCES
    ${EI_CPP}
    ${EI_DSP_CPP}
    ${EI_TFLITE_CPP}
    ${EI_PORT_CPP}
    "${CMAKE_CURRENT_LIST_DIR}/../tflite-model/tflite_learn_1085315_3_compiled.cpp"
)

idf_component_register(
    SRCS
        "warden_main.cpp"
        "warden_camera_stream.cpp"
        "swarm_link.cpp"
        "swarm_display.cpp"
        ${EI_SOURCES}
    INCLUDE_DIRS
        "."
        "../"
        "../components/edge_impulse"
        "../components/edge_impulse/edge-impulse-sdk"
        "../components/edge_impulse/edge-impulse-sdk/CMSIS/DSP/Include"
        "../components/edge_impulse/edge-impulse-sdk/porting/espressif"
        "../components/edge_impulse/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/include"
        "../components/edge_impulse/edge-impulse-sdk/porting/espressif/esp-dsp/modules/fft/include"
    REQUIRES
        esp32-camera
        esp_wifi
        esp_http_client
        esp_http_server
        esp_netif
        nvs_flash
        esp_timer
        json
        esp32_s3_eye
        esp_lvgl_port
        lvgl
    PRIV_REQUIRES
        esp_psram
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    EI_CLASSIFIER_USE_FULL_TFLITE=0
    EI_CLASSIFIER_PRINT_STATE=0
    EI_C_LINKAGE=1
    EI_PORTING_ESPRESSIF=1
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-sign-compare
    -Wno-missing-field-initializers
    -Wno-uninitialized
    -Wno-maybe-uninitialized
)