idf_component_register(
    SRC_DIRS 
        "tflite-model"
    INCLUDE_DIRS 
        "."
        "edge-impulse-sdk"
        "edge-impulse-sdk/classifier"
        "edge-impulse-sdk/dsp"
        "edge-impulse-sdk/dsp/image"
        "edge-impulse-sdk/porting"
        "edge-impulse-sdk/porting/espressif/ESP-NN/include"
        "edge-impulse-sdk/tensorflow"
        "edge-impulse-sdk/third_party"
        "edge-impulse-sdk/third_party/flatbuffers/include"
        "edge-impulse-sdk/third_party/ruy"
        "edge-impulse-sdk/third_party/gemmlowp"
        "edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/include"
        "edge-impulse-sdk/porting/espressif/esp-dsp/modules/fft/include"
        "model-parameters"
        "tflite-model"
    REQUIRES esp_timer
)

# Recursively collect all C, C++, and Assembly source files across the SDK
file(GLOB_RECURSE ei_sdk_sources 
    "edge-impulse-sdk/*.cpp"
    "edge-impulse-sdk/*.cc"
    "edge-impulse-sdk/*.c"
    "edge-impulse-sdk/*.S"
    "edge-impulse-sdk/*.s"
    "tflite-model/*.cpp"
)
target_sources(${COMPONENT_LIB} PRIVATE ${ei_sdk_sources})

# ESP-NN SIMD vector acceleration flags for ESP32-S3
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=1
    EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_S3=1
    EI_CLASSIFIER_USE_ESP_NN=1
    CONFIG_IDF_TARGET_ESP32S3=1
    __input_hw_esp32=1
)

# Maximum compiler optimizations (-O3, fast-math)
component_compile_options(
    -O3 
    -ffast-math
    -fstrict-volatile-bitfields
    -w
)