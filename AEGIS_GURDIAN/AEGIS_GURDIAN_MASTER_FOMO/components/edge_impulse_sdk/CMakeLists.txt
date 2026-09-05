idf_component_register(
    SRCS
        "tflite-model/tflite_learn_1043634_4_compiled.cpp"

    INCLUDE_DIRS
        "."
        "edge-impulse-sdk"
        "model-parameters"
        "tflite-model"

    REQUIRES
        esp-dsp
        esp_timer
)

file(GLOB_RECURSE EI_SOURCES
    "edge-impulse-sdk/*.cpp"
    "edge-impulse-sdk/*.cc"
    "edge-impulse-sdk/*.c"
)

target_sources(${COMPONENT_LIB} PRIVATE ${EI_SOURCES})

target_compile_options(${COMPONENT_LIB} PRIVATE
    -O3
    -ffast-math
    -Wno-error=cpp
    -Wno-error=missing-field-initializers
    -Wno-missing-field-initializers
    -Wno-error=uninitialized
    -Wno-error=maybe-uninitialized
    -Wno-uninitialized
    -Wno-maybe-uninitialized
    -Wno-deprecated-declarations
)