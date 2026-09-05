#pragma once

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// AEGIS BACKEND CONFIGURATION
// ============================================================

#define BACKEND_SERVER_URL "http://10.244.86.83:8000"
#define BACKEND_TELEMETRY_ENDPOINT "/api/v1/telemetry/ingest"
#define BACKEND_INCIDENT_ENDPOINT "/api/v1/incidents/report"
#define BACKEND_HEALTH_ENDPOINT "/"
#define BACKEND_SCENARIO_ENDPOINT "/api/v1/test/current-scenario"

#define BOT_ID "Warden"

// Wi-Fi Configuration
#define WIFI_SSID "A34"
#define WIFI_PASSWORD "01234567"
#define WIFI_MAXIMUM_RETRY 10
#define WIFI_CONNECT_TIMEOUT_MS 15000

// Telemetry Configuration
#define TELEMETRY_STATUS_INTERVAL_MS 3000
#define TELEMETRY_QUEUE_SIZE 20
#define HTTP_REQUEST_TIMEOUT_MS 5000

// ============================================================
// TELEMETRY MESSAGE TYPES
// ============================================================

typedef enum {
    TELEMETRY_MSG_STATUS,           // Periodic status update
    TELEMETRY_MSG_VISION,           // AI vision detection
    TELEMETRY_MSG_INCIDENT,         // Security incident
    TELEMETRY_MSG_FETCH_SCENARIO    // Request active scenario
} telemetry_msg_type_t;

// ============================================================
// VISION DETECTION DATA
// ============================================================

typedef struct {
    char label[32];           // "fire" or "smoke"
    float confidence;         // 0.0 - 1.0
    bool is_threat;           // Always true for fire/smoke
    int x, y, w, h;          // Bounding box (if available)
    int inference_time_ms;    // Total inference time
} vision_detection_t;

// ============================================================
// INCIDENT DATA
// ============================================================

typedef enum {
    INCIDENT_FIRE_DETECTED,
    INCIDENT_SMOKE_DETECTED,
    INCIDENT_FIRE_SMOKE_DETECTED,
    INCIDENT_SYSTEM_CLEAR,
    INCIDENT_SYSTEM_ERROR
} incident_type_t;

typedef enum {
    SEVERITY_LOW,
    SEVERITY_MEDIUM,
    SEVERITY_HIGH,
    SEVERITY_CRITICAL
} incident_severity_t;

typedef struct {
    incident_type_t type;
    incident_severity_t severity;
    char message[128];
    float fire_confidence;
    float smoke_confidence;
} incident_data_t;

// ============================================================
// STATUS DATA
// ============================================================

typedef struct {
    char status[32];          // "CLEAR", "FIRE", "SMOKE", "FIRE_SMOKE"
    uint32_t free_heap;
    uint32_t free_psram;
    int32_t wifi_rssi;
    char ip_address[16];
    uint32_t uptime_seconds;
    int fire_streak;
    int smoke_streak;
    int clear_streak;
} status_data_t;

// ============================================================
// TELEMETRY MESSAGE
// ============================================================

typedef struct {
    telemetry_msg_type_t type;
    union {
        status_data_t status;
        vision_detection_t vision;
        incident_data_t incident;
    } data;
} telemetry_msg_t;

// ============================================================
// PUBLIC API
// ============================================================

/**
 * Initialize backend communication system.
 * - Connects to Wi-Fi
 * - Creates telemetry queue
 * - Starts telemetry task
 * - Performs backend health check
 */
esp_err_t backend_init(void);

/**
 * Check if backend is reachable and operational.
 */
bool backend_is_online(void);

/**
 * Check if Wi-Fi is connected.
 */
bool backend_wifi_connected(void);

/**
 * Queue a status telemetry message (non-blocking).
 */
esp_err_t queue_status_telemetry(const status_data_t *status);

/**
 * Queue a vision detection message (non-blocking).
 */
esp_err_t queue_vision_detection(const vision_detection_t *detection);

/**
 * Queue an incident report (non-blocking).
 */
esp_err_t queue_incident_report(const incident_data_t *incident);

/**
 * Request the active scenario from backend (non-blocking).
 */
esp_err_t queue_fetch_scenario(void);

/**
 * Get the telemetry queue handle (for status checks).
 */
QueueHandle_t backend_get_queue(void);

#ifdef __cplusplus
}
#endif
