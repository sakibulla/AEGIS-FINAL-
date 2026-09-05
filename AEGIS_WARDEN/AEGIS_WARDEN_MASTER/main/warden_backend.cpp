#include "warden_backend.h"

#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_psram.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "cJSON.h"

static const char *TAG = "BACKEND";

// ============================================================
// WIFI EVENT BITS
// ============================================================

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group = NULL;
static int s_wifi_retry_count = 0;
static bool s_wifi_connected = false;

// ============================================================
// BACKEND STATE
// ============================================================

static bool s_backend_online = false;
static QueueHandle_t s_telemetry_queue = NULL;

// ============================================================
// WIFI EVENT HANDLER
// ============================================================

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT) {
        
        if (event_id == WIFI_EVENT_STA_START) {
            
            ESP_LOGI(TAG, "Wi-Fi station started, connecting...");
            esp_wifi_connect();
            
        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            
            s_wifi_connected = false;
            s_backend_online = false;
            
            if (s_wifi_retry_count < WIFI_MAXIMUM_RETRY) {
                esp_wifi_connect();
                s_wifi_retry_count++;
                ESP_LOGI(TAG, "Retry connection %d/%d", 
                    s_wifi_retry_count, WIFI_MAXIMUM_RETRY);
            } else {
                xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                ESP_LOGE(TAG, "Wi-Fi connection failed after %d retries", 
                    WIFI_MAXIMUM_RETRY);
            }
        }
        
    } else if (event_base == IP_EVENT) {
        
        if (event_id == IP_EVENT_STA_GOT_IP) {
            
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            
            ESP_LOGI(TAG, "Connected to Wi-Fi, IP: " IPSTR, 
                IP2STR(&event->ip_info.ip));
            
            s_wifi_retry_count = 0;
            s_wifi_connected = true;
            
            xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        }
    }
}

// ============================================================
// WIFI INITIALIZATION
// ============================================================

static esp_err_t wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        return ESP_FAIL;
    }
    
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
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        &wifi_event_handler,
        NULL,
        &instance_any_id));
    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        &wifi_event_handler,
        NULL,
        &instance_got_ip));
    
    wifi_config_t wifi_config = {};
    
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password) - 1);
    
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Wi-Fi initialization complete, SSID: %s", WIFI_SSID);
    
    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to SSID: %s", WIFI_SSID);
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to SSID: %s", WIFI_SSID);
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "Wi-Fi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

// ============================================================
// HTTP RESPONSE BUFFER
// ============================================================

#define HTTP_RESPONSE_BUFFER_SIZE 2048

typedef struct {
    char *buffer;
    size_t size;
    size_t offset;
} http_response_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_response_t *response = (http_response_t *)evt->user_data;
    
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (response && response->buffer && evt->data_len > 0) {
                size_t available = response->size - response->offset - 1;
                size_t to_copy = (evt->data_len < available) ? evt->data_len : available;
                
                if (to_copy > 0) {
                    memcpy(response->buffer + response->offset, evt->data, to_copy);
                    response->offset += to_copy;
                    response->buffer[response->offset] = '\0';
                }
            }
            break;
        default:
            break;
    }
    
    return ESP_OK;
}

// ============================================================
// HTTP GET REQUEST
// ============================================================

static esp_err_t backend_http_get(
    const char *url,
    char *response_buffer,
    size_t buffer_size,
    int *status_code)
{
    if (!s_wifi_connected) {
        ESP_LOGW(TAG, "Wi-Fi not connected, skipping GET");
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
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        if (status_code) {
            *status_code = code;
        }
        
        ESP_LOGI(TAG, "GET %s -> %d", url, code);
    } else {
        ESP_LOGE(TAG, "GET %s failed: %s", url, esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

// ============================================================
// HTTP POST REQUEST
// ============================================================

static esp_err_t backend_http_post(
    const char *url,
    const char *json_data,
    int *status_code)
{
    if (!s_wifi_connected) {
        ESP_LOGW(TAG, "Wi-Fi not connected, skipping POST");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (json_data == NULL) {
        ESP_LOGE(TAG, "POST data is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_REQUEST_TIMEOUT_MS
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));
    
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int code = esp_http_client_get_status_code(client);
        if (status_code) {
            *status_code = code;
        }
        
        if (code >= 200 && code < 300) {
            ESP_LOGI(TAG, "POST %s -> %d", url, code);
        } else {
            ESP_LOGW(TAG, "POST %s -> %d", url, code);
        }
    } else {
        ESP_LOGE(TAG, "POST %s failed: %s", url, esp_err_to_name(err));
    }
    
    esp_http_client_cleanup(client);
    return err;
}

// ============================================================
// BACKEND HEALTH CHECK
// ============================================================

static void backend_health_check(void)
{
    if (!s_wifi_connected) {
        return;
    }
    
    char response[512] = {0};
    int status_code = 0;
    
    char url[128];
    snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, BACKEND_HEALTH_ENDPOINT);
    
    esp_err_t err = backend_http_get(url, response, sizeof(response), &status_code);
    
    if (err == ESP_OK && status_code == 200) {
        s_backend_online = true;
        ESP_LOGI(TAG, "Backend health check: ONLINE");
    } else {
        s_backend_online = false;
        ESP_LOGW(TAG, "Backend health check: OFFLINE");
    }
}

// ============================================================
// FETCH ACTIVE SCENARIO
// ============================================================

static void backend_fetch_scenario(void)
{
    if (!s_backend_online) {
        return;
    }
    
    char response[HTTP_RESPONSE_BUFFER_SIZE] = {0};
    int status_code = 0;
    
    char url[128];
    snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, BACKEND_SCENARIO_ENDPOINT);
    
    esp_err_t err = backend_http_get(url, response, sizeof(response), &status_code);
    
    if (err == ESP_OK && status_code == 200) {
        
        cJSON *root = cJSON_Parse(response);
        if (root) {
            cJSON *scenario = cJSON_GetObjectItem(root, "scenario");
            if (cJSON_IsString(scenario)) {
                ESP_LOGI(TAG, "Active scenario: %s", scenario->valuestring);
            }
            cJSON_Delete(root);
        }
    } else {
        ESP_LOGW(TAG, "Failed to fetch scenario");
    }
}

// ============================================================
// SEND STATUS TELEMETRY
// ============================================================

static void send_status_telemetry(const status_data_t *status)
{
    if (!s_backend_online || status == NULL) {
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "kind", "telemetry");
    
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "status", status->status);
    
    // System info
    cJSON *system = cJSON_CreateObject();
    cJSON_AddNumberToObject(system, "free_heap", status->free_heap);
    cJSON_AddNumberToObject(system, "free_psram", status->free_psram);
    cJSON_AddItemToObject(payload, "system_info", system);
    
    // Network info
    cJSON_AddNumberToObject(payload, "wifi_rssi", status->wifi_rssi);
    cJSON_AddStringToObject(payload, "ip_address", status->ip_address);
    
    // Uptime
    char uptime_str[32];
    snprintf(uptime_str, sizeof(uptime_str), "uptime_%lus", status->uptime_seconds);
    cJSON_AddStringToObject(payload, "timestamp", uptime_str);
    
    // Detection state
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

// ============================================================
// SEND VISION DETECTION
// ============================================================

static void send_vision_detection(const vision_detection_t *detection)
{
    if (!s_backend_online || detection == NULL) {
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "kind", "vision_detection");
    
    cJSON *payload = cJSON_CreateObject();
    cJSON_AddStringToObject(payload, "label", detection->label);
    cJSON_AddNumberToObject(payload, "confidence", detection->confidence);
    cJSON_AddBoolToObject(payload, "is_threat", detection->is_threat);
    
    // Bounding box (if valid)
    if (detection->w > 0 && detection->h > 0) {
        cJSON *bbox = cJSON_CreateObject();
        cJSON_AddNumberToObject(bbox, "x", detection->x);
        cJSON_AddNumberToObject(bbox, "y", detection->y);
        cJSON_AddNumberToObject(bbox, "w", detection->w);
        cJSON_AddNumberToObject(bbox, "h", detection->h);
        cJSON_AddItemToObject(payload, "bbox", bbox);
    }
    
    // Inference timing
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

// ============================================================
// SEND INCIDENT REPORT
// ============================================================

static const char *incident_type_str(incident_type_t type)
{
    switch (type) {
        case INCIDENT_FIRE_DETECTED: return "FIRE_DETECTED";
        case INCIDENT_SMOKE_DETECTED: return "SMOKE_DETECTED";
        case INCIDENT_FIRE_SMOKE_DETECTED: return "FIRE_SMOKE_DETECTED";
        case INCIDENT_SYSTEM_CLEAR: return "SYSTEM_CLEAR";
        case INCIDENT_SYSTEM_ERROR: return "SYSTEM_ERROR";
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

static void send_incident_report(const incident_data_t *incident)
{
    if (!s_backend_online || incident == NULL) {
        return;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "type", incident_type_str(incident->type));
    cJSON_AddStringToObject(root, "message", incident->message);
    cJSON_AddStringToObject(root, "severity", severity_str(incident->severity));
    
    // Additional context
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

// ============================================================
// TELEMETRY TASK
// ============================================================

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
                    backend_fetch_scenario();
                    break;
                
                default:
                    ESP_LOGW(TAG, "Unknown telemetry message type: %d", msg.type);
                    break;
            }
        }
    }
}

// ============================================================
// PUBLIC API IMPLEMENTATION
// ============================================================

esp_err_t backend_init(void)
{
    ESP_LOGI(TAG, "Initializing backend communication...");
    ESP_LOGI(TAG, "Backend URL: %s", BACKEND_SERVER_URL);
    ESP_LOGI(TAG, "Bot ID: %s", BOT_ID);
    
    // Create telemetry queue
    s_telemetry_queue = xQueueCreate(TELEMETRY_QUEUE_SIZE, sizeof(telemetry_msg_t));
    if (s_telemetry_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create telemetry queue");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize Wi-Fi
    esp_err_t err = wifi_init_sta();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi initialization failed, continuing anyway...");
    }
    
    // Perform backend health check
    if (s_wifi_connected) {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Allow connection to stabilize
        backend_health_check();
        
        if (s_backend_online) {
            backend_fetch_scenario();
        }
    }
    
    // Start telemetry task
    BaseType_t result = xTaskCreatePinnedToCore(
        telemetry_task,
        "telemetry",
        4096,
        NULL,
        4,
        NULL,
        0  // Run on core 0 (AI task on core 1)
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create telemetry task");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Backend initialization complete");
    return ESP_OK;
}

bool backend_is_online(void)
{
    return s_backend_online;
}

bool backend_wifi_connected(void)
{
    return s_wifi_connected;
}

esp_err_t queue_status_telemetry(const status_data_t *status)
{
    if (s_telemetry_queue == NULL || status == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    telemetry_msg_t msg = {
        .type = TELEMETRY_MSG_STATUS,
        .data = { .status = *status }
    };
    
    if (xQueueSend(s_telemetry_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Telemetry queue full, dropping status message");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t queue_vision_detection(const vision_detection_t *detection)
{
    if (s_telemetry_queue == NULL || detection == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    telemetry_msg_t msg = {
        .type = TELEMETRY_MSG_VISION,
        .data = { .vision = *detection }
    };
    
    if (xQueueSend(s_telemetry_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Telemetry queue full, dropping vision message");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t queue_incident_report(const incident_data_t *incident)
{
    if (s_telemetry_queue == NULL || incident == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    telemetry_msg_t msg = {
        .type = TELEMETRY_MSG_INCIDENT,
        .data = { .incident = *incident }
    };
    
    if (xQueueSend(s_telemetry_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Telemetry queue full, dropping incident message");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

esp_err_t queue_fetch_scenario(void)
{
    if (s_telemetry_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    
    telemetry_msg_t msg = {
        .type = TELEMETRY_MSG_FETCH_SCENARIO
    };
    
    if (xQueueSend(s_telemetry_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Telemetry queue full, dropping scenario request");
        return ESP_ERR_NO_MEM;
    }
    
    return ESP_OK;
}

QueueHandle_t backend_get_queue(void)
{
    return s_telemetry_queue;
}
