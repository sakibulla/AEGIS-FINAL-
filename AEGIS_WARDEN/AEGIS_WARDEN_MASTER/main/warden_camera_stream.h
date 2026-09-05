#pragma once

#include "esp_err.h"
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize camera HTTP streaming server
 * 
 * Starts an HTTP server on port 81 that provides:
 * - /stream - MJPEG live video stream
 * - /capture - Single JPEG snapshot
 * - /status - Camera status JSON
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t camera_stream_init(void);

/**
 * @brief Stop camera streaming server
 */
void camera_stream_stop(void);

/**
 * @brief Get the stream URL
 * 
 * @return Stream URL string (e.g., "http://10.75.11.120:81/stream")
 */
const char* camera_stream_get_url(void);

/**
 * @brief Check if streaming server is running
 */
bool camera_stream_is_running(void);

/**
 * @brief Check if camera stream is currently active (someone viewing)
 */
bool camera_stream_is_active(void);

#ifdef __cplusplus
}
#endif
