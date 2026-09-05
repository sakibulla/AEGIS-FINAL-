#include <stdio.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_camera.h"
#include "esp_netif.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_system.h"

#include "cJSON.h"

#include "warden_config.h"
#include "warden_protocol.h"
#include "warden_camera_stream.h"
#include "swarm_link.h"
#include "swarm_display.h"
#include "bsp/esp-bsp.h"

#include "model-parameters/model_metadata.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

static const char *TAG = "WARDEN";

/* ============================================================
 * BACKEND CONFIGURATION
 * ============================================================ */

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

// Telemetry Configuration
#define TELEMETRY_STATUS_INTERVAL_MS 3000
#define TELEMETRY_QUEUE_SIZE 20
#define HTTP_REQUEST_TIMEOUT_MS 5000

/* ============================================================
 * BACKEND DATA TYPES
 * ============================================================ */

typedef enum {
    TELEMETRY_MSG_STATUS,
    TELEMETRY_MSG_VISION,
    TELEMETRY_MSG_INCIDENT,
    TELEMETRY_MSG_FETCH_SCENARIO
} telemetry_msg_type_t;

typedef struct {
    char label[32];
    float confidence;
    bool is_threat;
    int x, y, w, h;
    int inference_time_ms;
} vision_detection_t;

typedef enum {
    INCIDENT_FIRE_DETECTED,
    INCIDENT_SMOKE_DETECTED,
    INCIDENT_FIRE_SMOKE_DETECTED,
    INCIDENT_SYSTEM_CLEAR,
    INCIDENT_SYSTEM_ERROR,
    INCIDENT_GAS_DETECTED
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

typedef struct {
    char status[32];
    uint32_t free_heap;
    uint32_t free_psram;
    int32_t wifi_rssi;
    char ip_address[16];
    uint32_t uptime_seconds;
    int fire_streak;
    int smoke_streak;
    int clear_streak;
} status_data_t;

typedef struct {
    telemetry_msg_type_t type;
    union {
        status_data_t status;
        vision_detection_t vision;
        incident_data_t incident;
    } data;
} telemetry_msg_t;

/* ============================================================
 * BACKEND STATE
 * ============================================================ */

static int s_wifi_retry_count = 0;
static bool s_wifi_connected = false;
static bool s_backend_online = false;
static QueueHandle_t s_telemetry_queue = NULL;

/* ============================================================
 * BACKEND HTTP HELPER
 * ============================================================ */

typedef struct {
    char *buffer;
    size_t size;
    size_t offset;
} http_response_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_t *response = (http_response_t *)evt->user_data;
    
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (response && response->buffer && evt->data_len > 0) {
            size_t available = response->size - response->offset - 1;
            size_t to_copy = (evt->data_len < available) ? evt->data_len : available;
            
            if (to_copy > 0) {
                memcpy(response->buffer + response->offset, evt->data, to_copy);
                response->offset += to_copy;
                response->buffer[response->offset] = '\0';
            }
        }
    }
    
    return ESP_OK;
}

/* ============================================================
 * BACKEND FUNCTIONS - FORWARD DECLARATIONS
 * ============================================================ */

static void send_status_to_backend(warden_command_t, int, int, int);
static void send_vision_to_backend(const char *, float, int, const ei_impulse_result_bounding_box_t *);
static void send_incident_to_backend(incident_type_t, const char *, incident_severity_t, float, float);
static bool backend_wifi_connected(void);
static esp_err_t queue_status_telemetry(const status_data_t *);
static esp_err_t queue_vision_detection(const vision_detection_t *);
static esp_err_t queue_incident_report(const incident_data_t *);

/* ============================================================
 * WIFI EVENT HANDLER
 * ============================================================ */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Wi-Fi station started, connecting...");
            esp_wifi_connect();
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            s_wifi_connected = false;
            s_backend_online = false;
            // Keep the swarm ESP-NOW link alive on channel 6 while STA is unassociated
            // (SWARM_ESPNOW_DESIGN.md §2) — no effect once esp_wifi_connect() below re-associates.
            swarm_link_pin_channel();
            if (s_wifi_retry_count < WIFI_MAXIMUM_RETRY) {
                esp_wifi_connect();
                s_wifi_retry_count++;
                ESP_LOGI(TAG, "Retry connection %d/%d", s_wifi_retry_count, WIFI_MAXIMUM_RETRY);
            } else {
                ESP_LOGW(TAG, "Wi-Fi retry limit reached, backing off before trying again");
                s_wifi_retry_count = 0;
                vTaskDelay(pdMS_TO_TICKS(5000));
                esp_wifi_connect();
            }
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Connected to Wi-Fi, IP: " IPSTR, IP2STR(&event->ip_info.ip));
            s_wifi_retry_count = 0;
            s_wifi_connected = true;
        }
    }
}

/* ============================================================
 * WIFI INITIALIZATION
 * ============================================================ */

// Non-blocking: kicks off the connection attempt and returns immediately.
// Warden must be fully functional standalone (fire/smoke detection, on-device
// display, swarm ESP-NOW alerts) whether or not the backend AP is even in
// range — this used to block app_main() here for up to WIFI_CONNECT_TIMEOUT_MS
// (15s) via xEventGroupWaitBits before the camera server, display, or swarm
// link were even initialized. s_wifi_connected flips true asynchronously via
// IP_EVENT_STA_GOT_IP above, and every backend-facing call already checks it
// before doing anything — so there's nothing this wait was protecting.
static esp_err_t wifi_init_sta(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to create network interface");
        return ESP_FAIL;
    }
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));
    
    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi initialization complete, SSID: %s — connecting in background", WIFI_SSID);

    return ESP_OK;
}

/* ============================================================
 * HTTP POST/GET HELPERS
 * ============================================================ */

static esp_err_t backend_http_post(const char *url, const char *json_data, int *status_code)
{
    if (!s_wifi_connected || json_data == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_REQUEST_TIMEOUT_MS
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK && status_code) {
        *status_code = esp_http_client_get_status_code(client);
    }
    
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t backend_http_get(const char *url, char *response_buffer,
                                  size_t buffer_size, int *status_code)
{
    if (!s_wifi_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    
    http_response_t response = {
        .buffer = response_buffer,
        .size = buffer_size,
        .offset = 0
    };
    
    if (response_buffer && buffer_size > 0) {
        response_buffer[0] = '\0';
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_REQUEST_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .user_data = &response
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK && status_code) {
        *status_code = esp_http_client_get_status_code(client);
    }
    
    esp_http_client_cleanup(client);
    return err;
}

/* ============================================================
 * TELEMETRY SENDERS
 * ============================================================ */

static const char *incident_type_str(incident_type_t type)
{
    switch (type) {
        case INCIDENT_FIRE_DETECTED: return "FIRE_DETECTED";
        case INCIDENT_SMOKE_DETECTED: return "SMOKE_DETECTED";
        case INCIDENT_FIRE_SMOKE_DETECTED: return "FIRE_SMOKE_DETECTED";
        case INCIDENT_SYSTEM_CLEAR: return "SYSTEM_CLEAR";
        case INCIDENT_SYSTEM_ERROR: return "SYSTEM_ERROR";
        case INCIDENT_GAS_DETECTED: return "GAS_DETECTED";
        default: return "UNKNOWN";
    }
}

static const char *severity_str(incident_severity_t severity)
{
    switch (severity) {
        case SEVERITY_LOW: return "LOW";
        case SEVERITY_MEDIUM: return "MEDIUM";
        case SEVERITY_HIGH: return "HIGH";
        case SEVERITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

static void send_status_telemetry(const status_data_t *status)
{
    if (!s_wifi_connected || status == NULL) return;  // Changed from s_backend_online to s_wifi_connected
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "kind", "telemetry");
    
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "status", status->status);
    
    cJSON *system = cJSON_CreateObject();
    cJSON_AddNumberToObject(system, "free_heap", status->free_heap);
    cJSON_AddNumberToObject(system, "free_psram", status->free_psram);
    cJSON_AddItemToObject(payload, "system_info", system);
    
    cJSON_AddNumberToObject(payload, "wifi_rssi", status->wifi_rssi);
    cJSON_AddStringToObject(payload, "ip_address", status->ip_address);
    cJSON_AddNumberToObject(payload, "camera_port", 81);  // Camera stream port
    
    char uptime_str[32];
    snprintf(uptime_str, sizeof(uptime_str), "uptime_%lus", status->uptime_seconds);
    cJSON_AddStringToObject(payload, "timestamp", uptime_str);
    
    cJSON *detection = cJSON_CreateObject();
    cJSON_AddNumberToObject(detection, "fire_streak", status->fire_streak);
    cJSON_AddNumberToObject(detection, "smoke_streak", status->smoke_streak);
    cJSON_AddNumberToObject(detection, "clear_streak", status->clear_streak);
    cJSON_AddItemToObject(payload, "detection_state", detection);
    
    cJSON_AddItemToObject(root, "payload", payload);
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        char url[128];
        snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, BACKEND_TELEMETRY_ENDPOINT);
        backend_http_post(url, json_str, NULL);
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
}

static void send_vision_detection(const vision_detection_t *detection)
{
    if (!s_wifi_connected || detection == NULL) return;  // Changed from s_backend_online to s_wifi_connected
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "kind", "vision_detection");
    
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "label", detection->label);
    cJSON_AddNumberToObject(payload, "confidence", detection->confidence);
    cJSON_AddBoolToObject(payload, "is_threat", detection->is_threat);
    
    if (detection->w > 0 && detection->h > 0) {
        cJSON *bbox = cJSON_CreateObject();
        cJSON_AddNumberToObject(bbox, "x", detection->x);
        cJSON_AddNumberToObject(bbox, "y", detection->y);
        cJSON_AddNumberToObject(bbox, "w", detection->w);
        cJSON_AddNumberToObject(bbox, "h", detection->h);
        cJSON_AddItemToObject(payload, "bbox", bbox);
    }
    
    cJSON *timing = cJSON_CreateObject();
    cJSON_AddNumberToObject(timing, "total_ms", detection->inference_time_ms);
    cJSON_AddItemToObject(payload, "timing", timing);
    
    cJSON_AddItemToObject(root, "payload", payload);
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        char url[128];
        snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, BACKEND_TELEMETRY_ENDPOINT);
        backend_http_post(url, json_str, NULL);
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
}

static void send_incident_report(const incident_data_t *incident)
{
    if (!s_wifi_connected || incident == NULL) return;  // Changed from s_backend_online to s_wifi_connected
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "type", incident_type_str(incident->type));
    cJSON_AddStringToObject(root, "message", incident->message);
    cJSON_AddStringToObject(root, "severity", severity_str(incident->severity));
    
    cJSON *context = cJSON_CreateObject();
    cJSON_AddNumberToObject(context, "fire_confidence", incident->fire_confidence);
    cJSON_AddNumberToObject(context, "smoke_confidence", incident->smoke_confidence);
    cJSON_AddItemToObject(root, "context", context);
    
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        char url[128];
        snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, BACKEND_INCIDENT_ENDPOINT);
        backend_http_post(url, json_str, NULL);
        cJSON_free(json_str);
    }
    cJSON_Delete(root);
}

/* ============================================================
 * TELEMETRY TASK
 * ============================================================ */

static void telemetry_task(void *arg)
{
    (void)arg;
    telemetry_msg_t msg;
    
    ESP_LOGI(TAG, "Telemetry task started");
    
    while (true) {
        if (xQueueReceive(s_telemetry_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case TELEMETRY_MSG_STATUS:
                    send_status_telemetry(&msg.data.status);
                    break;
                case TELEMETRY_MSG_VISION:
                    send_vision_detection(&msg.data.vision);
                    break;
                case TELEMETRY_MSG_INCIDENT:
                    send_incident_report(&msg.data.incident);
                    break;
                case TELEMETRY_MSG_FETCH_SCENARIO:
                    // Scenario fetching can be added here if needed
                    break;
                default:
                    break;
            }
        }
    }
}

/* ============================================================
 * BACKEND QUEUE FUNCTIONS
 * ============================================================ */

static bool backend_wifi_connected(void)
{
    return s_wifi_connected;
}

static esp_err_t queue_status_telemetry(const status_data_t *status)
{
    if (s_telemetry_queue == NULL || status == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    telemetry_msg_t msg = {
        .type = TELEMETRY_MSG_STATUS,
        .data = { .status = *status }
    };
    
    if (xQueueSend(s_telemetry_queue, &msg, 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

static esp_err_t queue_vision_detection(const vision_detection_t *detection)
{
    if (s_telemetry_queue == NULL || detection == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    telemetry_msg_t msg = {
        .type = TELEMETRY_MSG_VISION,
        .data = { .vision = *detection }
    };
    
    if (xQueueSend(s_telemetry_queue, &msg, 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

static esp_err_t queue_incident_report(const incident_data_t *incident)
{
    if (s_telemetry_queue == NULL || incident == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    telemetry_msg_t msg = {
        .type = TELEMETRY_MSG_INCIDENT,
        .data = { .incident = *incident }
    };
    
    if (xQueueSend(s_telemetry_queue, &msg, 0) != pdTRUE) {
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

/* ============================================================
 * BACKEND INITIALIZATION
 * ============================================================ */

static esp_err_t backend_init(void)
{
    ESP_LOGI(TAG, "Initializing backend communication...");
    ESP_LOGI(TAG, "Backend URL: %s", BACKEND_SERVER_URL);
    ESP_LOGI(TAG, "Bot ID: %s", BOT_ID);
    
    s_telemetry_queue = xQueueCreate(TELEMETRY_QUEUE_SIZE, sizeof(telemetry_msg_t));
    if (s_telemetry_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create telemetry queue");
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t err = wifi_init_sta();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi initialization failed, continuing anyway...");
    }
    
    if (s_wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        // Health check
        char response[512] = {0};
        int status_code = 0;
        char url[128];
        snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, BACKEND_HEALTH_ENDPOINT);
        esp_err_t health_err = backend_http_get(url, response, sizeof(response), &status_code);
        if (health_err == ESP_OK && status_code == 200) {
            s_backend_online = true;
            ESP_LOGI(TAG, "Backend health check: ONLINE");
        } else {
            ESP_LOGW(TAG, "Backend health check: OFFLINE");
        }
    }
    
    BaseType_t result = xTaskCreatePinnedToCore(
        telemetry_task, "telemetry", 4096, NULL, 4, NULL, 0);
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Backend initialization complete");
    return ESP_OK;
}

/* ============================================================
 * ESP-NOW TARGET
 * ============================================================ */

#if WARDEN_USE_BROADCAST

static const uint8_t TX_MAC[6] = {
    0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF
};

#else

static const uint8_t TX_MAC[6] = WARDEN_SLAVE_MAC;

#endif

/* ============================================================
 * GLOBAL STATE
 * ============================================================ */

static uint16_t g_sequence = 0;

static warden_command_t g_last_command =
    WARDEN_CMD_CLEAR;

static int64_t g_last_alert_ms = 0;

// Gas is detected independently of the fire/smoke streak state machine
// above (it comes from the Slave's sensor, not Warden's own camera), so it
// gets its own bit OR'd into whatever gets broadcast to the swarm — see
// broadcast_combined_hazard() — rather than a separate send that would
// clobber whichever of fire/smoke/gas the swarm last heard about.
#define WARDEN_HAZARD_GAS_BIT 0x04
static volatile uint8_t g_gas_hazard_bit = 0;

// Broadcasts the combined fire/smoke/gas hazard state to the swarm.
// fire_smoke_bits mirrors warden_command_t (0=clear,1=fire,2=smoke,
// 3=fire+smoke); the gas bit (0x04) is OR'd in from the independent gas
// sensor state. Guardian/Pathfinder decode this combined byte on receipt
// (see swarm_link.cpp's SWARM_MSG_FIRE_ALERT case in all three projects).
static void broadcast_combined_hazard(uint8_t fire_smoke_bits, float fire_conf, float smoke_conf)
{
    uint8_t combined = fire_smoke_bits | g_gas_hazard_bit;
    if (combined == 0) {
        swarm_send_all_clear();
    } else {
        swarm_send_fire_alert(combined, fire_conf, smoke_conf);
    }
}
static int64_t g_last_status_telemetry_ms = 0;

static camera_fb_t *g_frame = nullptr;

// Track if incidents have been reported to avoid spam
static bool g_fire_incident_reported = false;
static bool g_smoke_incident_reported = false;
static bool g_fire_smoke_incident_reported = false;


/* ============================================================
 * CAMERA DATA CALLBACK
 *
 * Edge Impulse expects one RGB pixel packed as:
 *
 * 0x00RRGGBB
 *
 * Camera captures RGB565.
 * ============================================================ */

static int camera_get_data(
    size_t offset,
    size_t length,
    float *out_ptr)
{
    if (g_frame == nullptr || out_ptr == nullptr) {
        return -1;
    }

    /*
     * Edge Impulse model:
     *
     * 96 x 96 = 9216 pixels
     *
     * Camera:
     * QVGA = 320 x 240 RGB565
     *
     * We take a centered 96x96 crop from
     * the camera frame.
     */

const size_t model_width =EI_CLASSIFIER_INPUT_WIDTH;

const size_t model_height =EI_CLASSIFIER_INPUT_HEIGHT;

    const size_t camera_width =
        (size_t)g_frame->width;

    const size_t camera_height =
        (size_t)g_frame->height;

    if (camera_width < model_width ||
        camera_height < model_height) {
        return -1;
    }

    const size_t total_pixels =
        model_width * model_height;

    if (offset + length > total_pixels) {
        return -1;
    }

    /*
     * Center crop.
     */

    const size_t crop_x =
        (camera_width - model_width) / 2;

    const size_t crop_y =
        (camera_height - model_height) / 2;

    for (size_t i = 0; i < length; i++) {

        const size_t model_pixel =
            offset + i;

        const size_t model_x =
            model_pixel % model_width;

        const size_t model_y =
            model_pixel / model_width;

        const size_t camera_x =
            crop_x + model_x;

        const size_t camera_y =
            crop_y + model_y;

        const size_t camera_pixel =
            camera_y * camera_width + camera_x;

        const uint8_t *p =
            g_frame->buf + (camera_pixel * 2);

        const uint16_t rgb565 =
            ((uint16_t)p[0] << 8) |
            (uint16_t)p[1];

        const uint8_t r =
            (uint8_t)(
                (((rgb565 >> 11) & 0x1F) * 255) / 31
            );

        const uint8_t green =
            (uint8_t)(
                (((rgb565 >> 5) & 0x3F) * 255) / 63
            );

        const uint8_t b =
            (uint8_t)(
                ((rgb565 & 0x1F) * 255) / 31
            );

        const uint32_t rgb =
            ((uint32_t)r << 16) |
            ((uint32_t)green << 8) |
            (uint32_t)b;

        out_ptr[i] = (float)rgb;
    }

    return 0;
}


/* ============================================================
 * CAMERA INITIALIZATION
 * ESP32-S3-EYE / OV2640
 * ============================================================ */

static esp_err_t camera_init()
{
    camera_config_t config = {};

    config.ledc_channel =
        LEDC_CHANNEL_0;

    config.ledc_timer =
        LEDC_TIMER_0;

    /* Camera data pins */

    config.pin_d0 = 11;
    config.pin_d1 = 9;
    config.pin_d2 = 8;
    config.pin_d3 = 10;
    config.pin_d4 = 12;
    config.pin_d5 = 18;
    config.pin_d6 = 17;
    config.pin_d7 = 16;

    /* Camera clock/control */

    config.pin_xclk = 15;
    config.pin_pclk = 13;
    config.pin_vsync = 6;
    config.pin_href = 7;

    /* SCCB */

    config.pin_sccb_sda = 4;
    config.pin_sccb_scl = 5;

    config.pin_pwdn = -1;
    config.pin_reset = -1;

    config.xclk_freq_hz =
        20000000;  // 20MHz clock for high frame rate

    /*
     * Edge Impulse model expects RGB data.
     */

    config.pixel_format =
        PIXFORMAT_RGB565;

    /*
     * QVGA for balance of quality and speed.
     */

    config.frame_size =
        FRAMESIZE_QVGA;

    config.jpeg_quality = 10;  // Lower number = higher quality, 10-12 is good

    config.fb_count = 3;  // Use 3 buffers for smooth streaming

    config.fb_location =
        CAMERA_FB_IN_PSRAM;

    config.grab_mode =
        CAMERA_GRAB_WHEN_EMPTY;  // Better for concurrent access

    esp_err_t err =
        esp_camera_init(&config);

    if (err != ESP_OK) {

        ESP_LOGE(
            TAG,
            "Camera initialization failed: %s",
            esp_err_to_name(err)
        );

        return err;
    }

  sensor_t *sensor =
    esp_camera_sensor_get();

if (sensor != nullptr) {

    sensor->set_framesize(
        sensor,
        FRAMESIZE_QVGA
    );
}

ESP_LOGI(
    TAG,
    "OV2640 initialized: QVGA RGB565 -> 96x96 model crop"
);

    return ESP_OK;
}


/* ============================================================
 * PRINT MAC ADDRESS
 * ============================================================ */

static void print_mac(
    const char *name,
    const uint8_t *mac)
{
    ESP_LOGI(
        TAG,
        "%s: %02X:%02X:%02X:%02X:%02X:%02X",
        name,
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]
    );
}


/* ============================================================
 * ESP-NOW SEND CALLBACK
 *
 * ESP-IDF 5.5+:
 *
 * void (*)(const wifi_tx_info_t *,
 *          esp_now_send_status_t)
 * ============================================================ */

static void on_data_sent(
    const wifi_tx_info_t *tx_info,
    esp_now_send_status_t status)
{
    if (tx_info != nullptr) {
        const uint8_t *mac_addr = tx_info->des_addr;

        ESP_LOGI(
            TAG,
            "ESP-NOW [%02X:%02X:%02X:%02X:%02X:%02X] -> %s",
            mac_addr[0],
            mac_addr[1],
            mac_addr[2],
            mac_addr[3],
            mac_addr[4],
            mac_addr[5],
            status == ESP_NOW_SEND_SUCCESS
                ? "SUCCESS"
                : "FAIL"
        );

    } else {

        ESP_LOGI(
            TAG,
            "ESP-NOW send -> %s",
            status == ESP_NOW_SEND_SUCCESS
                ? "SUCCESS"
                : "FAIL"
        );
    }
}


/* ============================================================
 * ESP-NOW INITIALIZATION
 * ============================================================ */

static esp_err_t espnow_init()
{
    /*
     * Wi-Fi is already initialized by backend_init()
     * in WIFI_MODE_STA with AP connection.
     * 
     * We only need to set the ESP-NOW channel and
     * initialize ESP-NOW protocol.
     */

    uint8_t local_mac[6] = {};

    ESP_ERROR_CHECK(
        esp_wifi_get_mac(
            WIFI_IF_STA,
            local_mac
        )
    );

    print_mac(
        "Warden STA MAC",
        local_mac
    );

    /*
     * Get current Wi-Fi channel.
     * ESP-NOW will use the same channel as the AP connection.
     */

    uint8_t primary = 0;

    wifi_second_chan_t second =
        WIFI_SECOND_CHAN_NONE;

    ESP_ERROR_CHECK(
        esp_wifi_get_channel(
            &primary,
            &second
        )
    );

    ESP_LOGI(
        TAG,
        "ESP-NOW using Wi-Fi channel: %u (from AP connection)",
        primary
    );

    /*
     * Initialize ESP-NOW.
     */

    ESP_ERROR_CHECK(
        esp_now_init()
    );

    /*
     * Register send callback.
     */

    ESP_ERROR_CHECK(
        esp_now_register_send_cb(
            on_data_sent
        )
    );

    /*
     * Configure slave/broadcast peer.
     */

    esp_now_peer_info_t peer = {};

    memcpy(
        peer.peer_addr,
        TX_MAC,
        ESP_NOW_ETH_ALEN
    );

    peer.channel = 0;  // Use current channel

    peer.ifidx =
        WIFI_IF_STA;

    peer.encrypt = false;

    if (!esp_now_is_peer_exist(TX_MAC)) {

        ESP_ERROR_CHECK(
            esp_now_add_peer(&peer)
        );
    }

#if WARDEN_USE_BROADCAST

    ESP_LOGI(
        TAG,
        "ESP-NOW target: BROADCAST"
    );

#else

    print_mac(
        "ESP-NOW Slave",
        TX_MAC
    );

#endif

    return ESP_OK;
}


/* ============================================================
 * COMMAND NAME
 * ============================================================ */

static const char *command_name(
    warden_command_t command)
{
    switch (command) {

        case WARDEN_CMD_FIRE:
            return "FIRE";

        case WARDEN_CMD_SMOKE:
            return "SMOKE";

        case WARDEN_CMD_FIRE_SMOKE:
            return "FIRE+SMOKE";

        case WARDEN_CMD_CLEAR:
        default:
            return "CLEAR";
    }
}


/* ============================================================
 * SEND WARDEN COMMAND
 * ============================================================ */

static esp_err_t send_command(
    warden_command_t command,
    float fire_conf,
    float smoke_conf)
{
    warden_packet_t packet = {};

    packet.version =
        WARDEN_PROTOCOL_VERSION;

    packet.source =
        WARDEN_SOURCE_ID;

    packet.command =
        (uint8_t)command;

    packet.sequence =
        ++g_sequence;

    /*
     * Convert confidence:
     *
     * 0.00 -> 0
     * 1.00 -> 1000
     */

    packet.fire_confidence_x1000 =
        (uint16_t)fminf(
            fmaxf(
                fire_conf * 1000.0f,
                0.0f
            ),
            1000.0f
        );

    packet.smoke_confidence_x1000 =
        (uint16_t)fminf(
            fmaxf(
                smoke_conf * 1000.0f,
                0.0f
            ),
            1000.0f
        );

    esp_err_t err =
        esp_now_send(
            TX_MAC,
            (const uint8_t *)&packet,
            sizeof(packet)
        );

    if (err == ESP_OK) {

        ESP_LOGI(
            TAG,
            "TX -> %s | seq=%u | fire=%.2f | smoke=%.2f",
            command_name(command),
            packet.sequence,
            fire_conf,
            smoke_conf
        );

    } else {

        ESP_LOGE(
            TAG,
            "esp_now_send() failed: %s",
            esp_err_to_name(err)
        );
    }

    // Tell the swarm (Guardian/Pathfinder), independent of backend Wi-Fi state.
    // OR'd with the current gas bit so a fire/smoke transition doesn't erase
    // an active gas hazard from the swarm's view, or vice versa.
    broadcast_combined_hazard((uint8_t)command, fire_conf, smoke_conf);

    return err;
}


/* ============================================================
 * FORWARD DECLARATIONS
 * ============================================================ */

static void send_vision_to_backend(
    const char *label,
    float confidence,
    int inference_time_ms,
    const ei_impulse_result_bounding_box_t *box);

/* ============================================================
 * FIRE / SMOKE INFERENCE
 * ============================================================ */

static void run_fire_smoke_inference(
    float *fire_conf,
    float *smoke_conf,
    bool *fire_detected,
    bool *smoke_detected)
{
    *fire_conf = 0.0f;
    *smoke_conf = 0.0f;

    *fire_detected = false;
    *smoke_detected = false;

    /*
     * Create Edge Impulse signal.
     */

    signal_t signal = {};

    signal.total_length =
        EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

    signal.get_data =
        camera_get_data;

    ei_impulse_result_t result = {};

    EI_IMPULSE_ERROR err =
        run_classifier(
            &signal,
            &result,
            false
        );

    if (err != EI_IMPULSE_OK) {

        ESP_LOGE(
            TAG,
            "Inference failed: %d",
            err
        );

        return;
    }

#if EI_CLASSIFIER_OBJECT_DETECTION == 1

    /*
     * Object detection model.
     */

    for (uint32_t i = 0;
         i < result.bounding_boxes_count;
         ++i) {

        const auto &box =
            result.bounding_boxes[i];

        if (box.value <= 0.0f) {
            continue;
        }

        if (box.value <
            WARDEN_DETECTION_THRESHOLD) {
            continue;
        }

        /*
         * FIRE
         */

        if (strcmp(box.label, "fire") == 0) {

            if (box.value > *fire_conf) {
                *fire_conf = box.value;
            }
        }

        /*
         * SMOKE
         */

        else if (
            strcmp(box.label, "smoke") == 0) {

            if (box.value > *smoke_conf) {
                *smoke_conf = box.value;
            }
        }
    }

#endif

    *fire_detected =
        (*fire_conf >=
         WARDEN_DETECTION_THRESHOLD);

    *smoke_detected =
        (*smoke_conf >=
         WARDEN_DETECTION_THRESHOLD);

    int total_inference_time =
        result.timing.dsp +
        result.timing.classification +
        result.timing.postprocessing;

    ESP_LOGI(
        TAG,
        "Inference | DSP=%dms NN=%dms POST=%dms | FIRE=%.2f | SMOKE=%.2f",
        result.timing.dsp,
        result.timing.classification,
        result.timing.postprocessing,
        *fire_conf,
        *smoke_conf
    );

    // Send vision detections to backend
    // Only send detections that meet threshold to avoid spam
#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    for (uint32_t i = 0;
         i < result.bounding_boxes_count;
         ++i) {

        const auto &box =
            result.bounding_boxes[i];

        if (box.value <= 0.0f ||
            box.value < WARDEN_DETECTION_THRESHOLD) {
            continue;
        }

        // Send to backend
        send_vision_to_backend(
            box.label,
            box.value,
            total_inference_time,
            &box
        );
    }
#endif
}


/* ============================================================
 * COMMAND DECISION
 * ============================================================ */

static warden_command_t choose_command(
    int fire_streak,
    int smoke_streak)
{
    /*
     * FIRE + SMOKE
     */

    if (fire_streak >=
            WARDEN_DETECTION_FRAMES &&
        smoke_streak >=
            WARDEN_DETECTION_FRAMES) {

        return WARDEN_CMD_FIRE_SMOKE;
    }

    /*
     * FIRE
     */

    if (fire_streak >=
        WARDEN_DETECTION_FRAMES) {

        return WARDEN_CMD_FIRE;
    }

    /*
     * SMOKE
     */

    if (smoke_streak >=
        WARDEN_DETECTION_FRAMES) {

        return WARDEN_CMD_SMOKE;
    }

    /*
     * Nothing detected.
     */

    return WARDEN_CMD_CLEAR;
}


/* ============================================================
 * BACKEND TELEMETRY HELPERS
 * ============================================================ */

static void send_status_to_backend(
    warden_command_t current_command,
    int fire_streak,
    int smoke_streak,
    int clear_streak)
{
    if (!backend_wifi_connected()) {
        return;
    }
    
    status_data_t status = {};
    
    // Convert command to status string
    switch (current_command) {
        case WARDEN_CMD_FIRE:
            strncpy(status.status, "FIRE", sizeof(status.status) - 1);
            break;
        case WARDEN_CMD_SMOKE:
            strncpy(status.status, "SMOKE", sizeof(status.status) - 1);
            break;
        case WARDEN_CMD_FIRE_SMOKE:
            strncpy(status.status, "FIRE_SMOKE", sizeof(status.status) - 1);
            break;
        case WARDEN_CMD_CLEAR:
        default:
            strncpy(status.status, "CLEAR", sizeof(status.status) - 1);
            break;
    }
    
    // System info
    status.free_heap = esp_get_free_heap_size();
    status.free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    
    // Wi-Fi info
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        status.wifi_rssi = ap_info.rssi;
    } else {
        status.wifi_rssi = 0;
    }
    
    // IP address
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            snprintf(status.ip_address, sizeof(status.ip_address),
                IPSTR, IP2STR(&ip_info.ip));
        }
    }
    
    // Uptime
    status.uptime_seconds = (uint32_t)(esp_timer_get_time() / 1000000);
    
    // Detection state
    status.fire_streak = fire_streak;
    status.smoke_streak = smoke_streak;
    status.clear_streak = clear_streak;
    
    queue_status_telemetry(&status);
}

static void send_vision_to_backend(
    const char *label,
    float confidence,
    int inference_time_ms,
    const ei_impulse_result_bounding_box_t *box)
{
    if (!backend_wifi_connected()) {
        return;
    }
    
    vision_detection_t detection = {};
    
    strncpy(detection.label, label, sizeof(detection.label) - 1);
    detection.confidence = confidence;
    detection.is_threat = true; // Fire and smoke are always threats
    detection.inference_time_ms = inference_time_ms;
    
    // Bounding box (if available)
    if (box) {
        detection.x = (int)box->x;
        detection.y = (int)box->y;
        detection.w = (int)box->width;
        detection.h = (int)box->height;
    } else {
        detection.x = 0;
        detection.y = 0;
        detection.w = 0;
        detection.h = 0;
    }
    
    queue_vision_detection(&detection);
}

static void send_incident_to_backend(
    incident_type_t type,
    const char *message,
    incident_severity_t severity,
    float fire_conf,
    float smoke_conf)
{
    if (!backend_wifi_connected()) {
        return;
    }
    
    incident_data_t incident = {};
    
    incident.type = type;
    incident.severity = severity;
    strncpy(incident.message, message, sizeof(incident.message) - 1);
    incident.fire_confidence = fire_conf;
    incident.smoke_confidence = smoke_conf;

    queue_incident_report(&incident);
}

/* ============================================================
 * SLAVE GAS SENSOR STATUS (Master<->Slave link, Slave -> Master)
 *
 * Registered with swarm_link_set_other_frame_cb() — swarm_link owns the one
 * ESP-NOW recv callback slot on this board, so any frame that isn't a swarm
 * packet (checked first) lands here instead. This is the first time this
 * board has ever received anything from its own Slave; previously that link
 * was send-only.
 * ============================================================ */

static uint16_t s_last_gas_sequence = 0;
static bool s_gas_sequence_seen = false;

static void handle_slave_gas_status(const uint8_t *data, int len)
{
    if (data == nullptr || len != (int)sizeof(warden_slave_status_t)) {
        return; // not this packet shape
    }

    warden_slave_status_t status = {};
    memcpy(&status, data, sizeof(status));

    if (status.magic != WARDEN_SLAVE_STATUS_MAGIC || status.source != WARDEN_SLAVE_ID) {
        return;
    }

    if (s_gas_sequence_seen && status.sequence == s_last_gas_sequence) {
        return; // duplicate/retransmit
    }
    s_last_gas_sequence = status.sequence;
    s_gas_sequence_seen = true;

    ESP_LOGW(
        TAG,
        "Slave gas status: %s (ADC=%u/4095)",
        status.gas_detected ? "DETECTED" : "clear",
        status.gas_value
    );

    uint8_t new_gas_bit = status.gas_detected ? WARDEN_HAZARD_GAS_BIT : 0;
    if (new_gas_bit != g_gas_hazard_bit) {
        g_gas_hazard_bit = new_gas_bit;
        // Re-broadcast combined state — current fire/smoke bits (g_last_command
        // already mirrors WARDEN_CMD_*) OR'd with the new gas bit — so this
        // doesn't clobber an active fire/smoke condition the swarm already
        // knows about, or get clobbered by the next fire/smoke tick.
        broadcast_combined_hazard((uint8_t)g_last_command, 0.0f, 0.0f);
    }

    if (status.gas_detected) {
        char msg[96];
        snprintf(msg, sizeof(msg), "Gas detected by Warden Slave sensor (ADC=%u/4095)", status.gas_value);
        send_incident_to_backend(INCIDENT_GAS_DETECTED, msg, SEVERITY_CRITICAL, 0.0f, 0.0f);
    }
}


/* ============================================================
 * WARDEN MASTER TASK
 * ============================================================ */

/*
 * Sends status telemetry if TELEMETRY_STATUS_INTERVAL_MS has elapsed since
 * the last send. Previously this check only ran once per full pass through
 * warden_master_task's loop — and that loop either blocks for
 * WARDEN_FRAME_DELAY_MS (10s) between inference cycles, or skips straight
 * past the check entirely via `continue` whenever the camera stream is being
 * viewed. Net effect: telemetry could go many times longer than the intended
 * 3s between updates, or stop completely for as long as someone had the
 * stream open. Calling this on every ~500ms tick of the loop (see below)
 * instead of once per inference cycle decouples telemetry cadence from both
 * of those.
 */
static void maybe_send_status_telemetry(int fire_streak, int smoke_streak, int clear_streak)
{
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - g_last_status_telemetry_ms >= TELEMETRY_STATUS_INTERVAL_MS) {
        send_status_to_backend(g_last_command, fire_streak, smoke_streak, clear_streak);
        g_last_status_telemetry_ms = now_ms;
    }
}

static void warden_master_task(
    void *arg)
{
    (void)arg;

    int fire_streak = 0;
    int smoke_streak = 0;
    int clear_streak = 0;

    while (true) {

        /*
         * Check if camera stream is active.
         * If someone is viewing the stream, pause AI to give camera full bandwidth.
         */
        if (camera_stream_is_active()) {
            // Stream is active, skip AI inference — but status telemetry must
            // keep flowing regardless (it previously stopped entirely here).
            maybe_send_status_telemetry(fire_streak, smoke_streak, clear_streak);
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        /*
         * Capture frame.
         */

        g_frame =
            esp_camera_fb_get();

        if (g_frame == nullptr) {

            ESP_LOGE(
                TAG,
                "Camera capture failed"
            );

            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

        float fire_conf = 0.0f;
        float smoke_conf = 0.0f;

        bool fire = false;
        bool smoke = false;

        /*
         * Run AI.
         */

        run_fire_smoke_inference(
            &fire_conf,
            &smoke_conf,
            &fire,
            &smoke
        );

        /*
         * Release frame.
         */

        esp_camera_fb_return(
            g_frame
        );

        g_frame = nullptr;

        /*
         * Update detection streaks.
         */

        if (fire) {
            fire_streak++;
        } else {
            fire_streak = 0;
        }

        if (smoke) {
            smoke_streak++;
        } else {
            smoke_streak = 0;
        }

        if (!fire && !smoke) {
            clear_streak++;
        } else {
            clear_streak = 0;
        }

        /*
         * Decide command.
         */

        warden_command_t command =
            choose_command(
                fire_streak,
                smoke_streak
            );

        const int64_t now_ms =
            esp_timer_get_time() / 1000;

        /*
         * Send FIRE / SMOKE alert.
         */

        if (command !=
            WARDEN_CMD_CLEAR) {

            if (command !=
                    g_last_command ||
                now_ms -
                    g_last_alert_ms >=
                    WARDEN_ALERT_REPEAT_MS) {

                send_command(
                    command,
                    fire_conf,
                    smoke_conf
                );

                g_last_command =
                    command;

                g_last_alert_ms =
                    now_ms;

                clear_streak = 0;

                // Send incident to backend (first occurrence only)
                if (command == WARDEN_CMD_FIRE && !g_fire_incident_reported) {
                    send_incident_to_backend(
                        INCIDENT_FIRE_DETECTED,
                        "Fire detected by vision system",
                        SEVERITY_CRITICAL,
                        fire_conf,
                        smoke_conf
                    );
                    g_fire_incident_reported = true;
                    g_smoke_incident_reported = false;
                    g_fire_smoke_incident_reported = false;
                }
                else if (command == WARDEN_CMD_SMOKE && !g_smoke_incident_reported) {
                    send_incident_to_backend(
                        INCIDENT_SMOKE_DETECTED,
                        "Smoke detected by vision system",
                        SEVERITY_HIGH,
                        fire_conf,
                        smoke_conf
                    );
                    g_smoke_incident_reported = true;
                    g_fire_incident_reported = false;
                    g_fire_smoke_incident_reported = false;
                }
                else if (command == WARDEN_CMD_FIRE_SMOKE && !g_fire_smoke_incident_reported) {
                    send_incident_to_backend(
                        INCIDENT_FIRE_SMOKE_DETECTED,
                        "Fire and smoke detected by vision system",
                        SEVERITY_CRITICAL,
                        fire_conf,
                        smoke_conf
                    );
                    g_fire_smoke_incident_reported = true;
                    g_fire_incident_reported = false;
                    g_smoke_incident_reported = false;
                }
            }
        }

        /*
         * Send CLEAR after
         * sustained no-detection.
         */

        if (clear_streak >=
                WARDEN_CLEAR_FRAMES &&
            g_last_command !=
                WARDEN_CMD_CLEAR) {

            send_command(
                WARDEN_CMD_CLEAR,
                0.0f,
                0.0f
            );

            g_last_command =
                WARDEN_CMD_CLEAR;

            g_last_alert_ms =
                now_ms;

            fire_streak = 0;
            smoke_streak = 0;
            clear_streak = 0;

            // Send clear incident to backend
            if (g_fire_incident_reported || 
                g_smoke_incident_reported || 
                g_fire_smoke_incident_reported) {
                
                send_incident_to_backend(
                    INCIDENT_SYSTEM_CLEAR,
                    "System returned to clear state",
                    SEVERITY_LOW,
                    0.0f,
                    0.0f
                );
                
                g_fire_incident_reported = false;
                g_smoke_incident_reported = false;
                g_fire_smoke_incident_reported = false;
            }
        }

        /*
         * Periodic status telemetry to backend (every 3 seconds).
         */

        maybe_send_status_telemetry(fire_streak, smoke_streak, clear_streak);

        /*
         * Detection loop delay. Chunked into short ticks (instead of one
         * long vTaskDelay(WARDEN_FRAME_DELAY_MS) block) so the telemetry
         * check above still gets evaluated on its own ~3s cadence during
         * this wait, without changing the actual pacing between inference
         * cycles — the total wait is still WARDEN_FRAME_DELAY_MS either way.
         */

        {
            int remaining_ms = WARDEN_FRAME_DELAY_MS;
            const int tick_ms = 500;
            while (remaining_ms > 0) {
                int this_tick = (remaining_ms < tick_ms) ? remaining_ms : tick_ms;
                vTaskDelay(pdMS_TO_TICKS(this_tick));
                remaining_ms -= this_tick;
                maybe_send_status_telemetry(fire_streak, smoke_streak, clear_streak);
            }
        }
    }
}


/* ============================================================
 * SWARM: WAKE / SCOUT-DELAY HOOKS (SWARM_ESPNOW_DESIGN.md §8a)
 *
 * Guardian's WAKE arms a 5-minute one-shot timer, same as Pathfinder. Warden
 * has no existing patrol/movement state machine today — its fire/smoke
 * inference loop (warden_master_task) runs continuously and intentionally is
 * NOT gated on this, since delaying detection behind a wake timer would be a
 * safety regression, not "scouting". s_scout_armed is wired and available for
 * whoever adds real Warden movement/patrol behavior later.
 * ============================================================ */

static esp_timer_handle_t s_scout_delay_timer = nullptr;
static volatile bool s_scout_armed = false;

static void scout_delay_timer_cb(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "Swarm: 5-minute scout delay elapsed");
    s_scout_armed = true;
}

static void on_swarm_wake(void)
{
    if (s_scout_armed) {
        return;
    }
    if (s_scout_delay_timer == nullptr) {
        const esp_timer_create_args_t args = {
            .callback = &scout_delay_timer_cb,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "scout_delay",
        };
        esp_timer_create(&args, &s_scout_delay_timer);
    } else if (esp_timer_is_active(s_scout_delay_timer)) {
        esp_timer_stop(s_scout_delay_timer);
    }
    ESP_LOGI(TAG, "Swarm: WAKE received -> scout delay armed (5 minutes)");
    esp_timer_start_once(s_scout_delay_timer, 5ULL * 60ULL * 1000000ULL);
}

static void on_swarm_all_clear(void)
{
    if (s_scout_delay_timer != nullptr && esp_timer_is_active(s_scout_delay_timer)) {
        esp_timer_stop(s_scout_delay_timer);
        ESP_LOGI(TAG, "Swarm: ALL_CLEAR received -> scout delay cancelled");
    }
}

/* ============================================================
 * APPLICATION ENTRY
 * ============================================================ */

extern "C" void app_main(void)
{
    ESP_LOGI(
        TAG,
        "========================================"
    );

    ESP_LOGI(
        TAG,
        "       A.E.G.I.S. WARDEN MASTER"
    );

    ESP_LOGI(
        TAG,
        "       FIRE + SMOKE AI NODE"
    );

    ESP_LOGI(
        TAG,
        "========================================"
    );

    /*
     * Model information.
     */

    ESP_LOGI(
        TAG,
        "Project: %s",
        EI_CLASSIFIER_PROJECT_NAME
    );

    ESP_LOGI(
        TAG,
        "Input: %dx%d",
        EI_CLASSIFIER_INPUT_WIDTH,
        EI_CLASSIFIER_INPUT_HEIGHT
    );

    ESP_LOGI(
        TAG,
        "Raw samples: %d",
        EI_CLASSIFIER_RAW_SAMPLE_COUNT
    );

    ESP_LOGI(
        TAG,
        "TFLite arena: %d bytes",
        EI_CLASSIFIER_TFLITE_LARGEST_ARENA_SIZE
    );

    ESP_LOGI(
        TAG,
        "Detection threshold: %.2f",
        WARDEN_DETECTION_THRESHOLD
    );

    /*
     * Initialize NVS.
     */

    esp_err_t ret =
        nvs_flash_init();

    if (ret ==
            ESP_ERR_NVS_NO_FREE_PAGES ||
        ret ==
            ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ret =
            nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    /*
     * Camera.
     */

    ESP_ERROR_CHECK(
        camera_init()
    );

    /*
     * Backend communication (Wi-Fi + Telemetry).
     * 
     * Note: This changes Wi-Fi mode from WIFI_MODE_STA
     * used by ESP-NOW to full station mode with AP connection.
     * ESP-NOW will be re-initialized after this.
     */

    ESP_LOGI(TAG, "Initializing backend communication...");
    
    esp_err_t backend_err = backend_init();
    if (backend_err != ESP_OK) {
        ESP_LOGW(TAG, "Backend initialization failed, continuing...");
    }

    /*
     * ESP-NOW.
     * 
     * Re-initialize ESP-NOW since backend_init() sets up
     * Wi-Fi in station mode with AP connection.
     */

    ESP_ERROR_CHECK(
        espnow_init()
    );

    // Swarm on-device status display (SWARM_ESPNOW_DESIGN.md §11) — bring up
    // the integrated LCD/LVGL stack before the swarm link, so swarm_link can
    // push updates to it as soon as it starts hearing from siblings.
    // Explicit config, Warden-specific: Guardian/Pathfinder put this buffer in
    // internal SRAM (fixes a visual tearing issue seen there). On Warden that
    // choice cost ~19KB of internal SRAM that turned out to be load-bearing —
    // warden_master_task's 16KB stack allocation started silently failing
    // (xTaskCreatePinnedToCore returning an unchecked error), because Warden
    // also runs a full-time camera stream server + a 185KB TFLite arena on
    // top of Wi-Fi/ESP-NOW, all competing for that same small internal pool.
    // Fire/smoke detection actually running takes priority over the cosmetic
    // tearing risk here, so this buffer goes back to PSRAM for Warden only —
    // revisit if tearing shows up on Warden's screen specifically.
    //
    // Moving the draw buffer alone freed ~19KB but still left only 12288
    // bytes of internal SRAM free — still short of the 16384-byte stack the
    // AI task needs. The LVGL task's own execution stack (separate from the
    // draw buffer) defaults to a *hardcoded* 7168 bytes of internal SRAM
    // regardless of buff_spiram, via task_stack_caps — so it's moved to
    // PSRAM here too (esp_lvgl_port explicitly supports this via
    // xTaskCreatePinnedToCoreWithCaps). That should free the full 7168 bytes
    // back to internal SRAM, giving ~19KB headroom for the 16KB task stack.
    bsp_display_cfg_t disp_cfg = {
        .lvgl_port_cfg = {
            .task_priority = 4,
            .task_stack = 7168,
            .task_affinity = -1,
            .task_max_sleep_ms = 500,
            .task_stack_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
            .timer_period_ms = 5,
        },
        .buffer_size = BSP_LCD_H_RES * 40,
        .double_buffer = false,
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
        }
    };
    lv_display_t *swarm_disp = bsp_display_start_with_config(&disp_cfg);
    if (swarm_disp != nullptr) {
        // bsp_display_start_with_config() already started the LVGL port task,
        // which can be mid-render on its own thread by the time we get here.
        // Calling into LVGL (rotate, or anything under swarm_display_init())
        // without holding the lock races lv_obj_invalidate() against that
        // render pass; if it loses, LVGL's LV_ASSERT_HANDLER (a bare
        // `while(1);`) silently hangs app_main() forever, which starves
        // IDLE0 and shows up only as a task watchdog reset with no log line.
        if (bsp_display_lock(1000)) {
            // Physical panel is mounted upside-down relative to how the BSP
            // assumes it's oriented by default.
            bsp_display_rotate(swarm_disp, LV_DISPLAY_ROTATION_180);
            // The BSP configures the backlight PWM channel at 0% duty (off) and
            // never turns it on itself — without this call, LVGL renders fine but
            // nothing is ever visible.
            bsp_display_backlight_on();
            bsp_display_unlock();
        } else {
            ESP_LOGW(TAG, "bsp_display_lock() timed out; skipping display rotate/backlight");
        }
        swarm_display_init();
    } else {
        ESP_LOGW(TAG, "bsp_display_start_with_config() failed; swarm status will not be shown on-screen");
    }

    // Swarm ESP-NOW link (Master-to-Master, independent of the Master<->Slave
    // link above) — see SWARM_ESPNOW_DESIGN.md.
    if (swarm_link_init() == ESP_OK) {
        swarm_link_set_wake_cb(on_swarm_wake);
        swarm_link_set_all_clear_cb(on_swarm_all_clear);
        swarm_link_set_other_frame_cb(handle_slave_gas_status);
    }

    /*
     * Camera HTTP streaming server.
     * 
     * Starts HTTP server on port 81 for live video feed.
     * Frontend can access: http://[WARDEN_IP]:81/stream
     */

    ESP_LOGI(TAG, "Initializing camera streaming server...");
    
    esp_err_t stream_err = camera_stream_init();
    if (stream_err != ESP_OK) {
        ESP_LOGW(TAG, "Camera stream initialization failed, continuing...");
    } else {
        // Live /stream is intentionally disabled (see warden_camera_stream.cpp)
        // — snapshot mode only, so this is no longer a reachable URL.
        ESP_LOGI(TAG, "Camera up in snapshot-only mode (no live /stream): %s", camera_stream_get_url());
    }

    ESP_LOGI(
        TAG,
        "========================================"
    );

    ESP_LOGI(
        TAG,
        "Warden Master READY"
    );

    ESP_LOGI(
        TAG,
        "Watching for FIRE / SMOKE..."
    );

    ESP_LOGI(
        TAG,
        "========================================"
    );

    /*
     * Start AI task.
     */

    BaseType_t warden_task_result = xTaskCreatePinnedToCore(
        warden_master_task,
        "warden_master",
        16384,
        nullptr,
        5,
        nullptr,
        1
    );

    if (warden_task_result != pdPASS) {
        // This was previously unchecked, so a creation failure (e.g. not
        // enough free heap for the 16KB stack) was completely silent — no
        // log, no crash, the fire/smoke AI task simply never existed. Make
        // it visible instead of a silent no-op.
        ESP_LOGE(
            TAG,
            "FAILED to create warden_master_task (result=%d) — fire/smoke detection will NOT run! "
            "Free heap (all regions, incl. PSRAM): %u, largest free INTERNAL block (what a task stack actually needs): %u",
            (int)warden_task_result,
            (unsigned)esp_get_free_heap_size(),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
        );
    }
}