idf_component_register(
    SRCS
        "vision_task.cpp"
        "web_streamer.cpp"
        "espnow_mesh.cpp"
        "swarm_link.cpp"
        "swarm_display.cpp"
        "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        esp_camera
        edge_impulse
        nvs_flash
        esp_wifi
        esp_http_server
        esp_http_client
        esp_pm
        esp_timer
        esp32_s3_eye
        esp_lvgl_port
        lvgl
)

# Enable S3 Vector SIMD math and suppress third-party template warnings
component_compile_options(
    -O3
    -ffast-math
    -mlongcalls
    -Wno-error=cpp
    -Wno-error=missing-field-initializers
    -Wno-missing-field-initializers
    -Wno-error=uninitialized
    -Wno-error=maybe-uninitialized
    -Wno-uninitialized
    -Wno-maybe-uninitialized
)