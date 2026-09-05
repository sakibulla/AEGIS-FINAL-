#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <list>

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "esp_http_server.h"
#include "esp_http_client.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_pm.h"
#include "espnow_master.h"
#include "swarm_link.h"
#include "swarm_display.h"
#include "bsp/esp-bsp.h"
#include "cJSON.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/idf_additions.h"

#define EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN 1
#define EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN_S3 1
#define EI_CLASSIFIER_USE_ESP_NN 1
#define EI_CLASSIFIER_USE_FULL_TFLITE 0

#ifndef CONFIG_IDF_TARGET_ESP32S3
#define CONFIG_IDF_TARGET_ESP32S3
#endif

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include "dl_image_define.hpp"

#define ENABLE_VOICE_ACTIVATION 1

#if ENABLE_VOICE_ACTIVATION
#include "voice_activation.h"
#endif

static const char *TAG = "AEGIS_GUARDIAN";

#define CAM_WIDTH 640
#define CAM_HEIGHT 480

#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y9_GPIO_NUM 16
#define Y8_GPIO_NUM 17
#define Y7_GPIO_NUM 18
#define Y6_GPIO_NUM 12
#define Y5_GPIO_NUM 10
#define Y4_GPIO_NUM 8
#define Y3_GPIO_NUM 9
#define Y2_GPIO_NUM 11
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

#define CAMERA_XCLK_HZ 20000000
#define CAMERA_JPEG_QUALITY 12
#define CAMERA_FB_COUNT 3

#define STREAM_DELAY_MS 5
#define FACE_INTERVAL_MS 1200
#define OBJECT_INTERVAL_MS 1500

#define BUF_ALIGN 16

#define AUTO_ENROLL_FIRST_FACE 1

#define OWNER_SIMILARITY_THRESHOLD 0.50f
#define OWNER_CONFIRM_COUNT 2
#define INTRUDER_CONFIRM_COUNT 3

#define OBJECT_THRESHOLD 0.50f

#define ESPNOW_CHANNEL 6

// ============================================================
// AEGIS Backend Server & WiFi Configuration
// ============================================================
#define BOT_ID                     "Guardian"
#define BACKEND_SERVER_URL         "http://10.244.86.83:8000"
#define BACKEND_TELEMETRY_ENDPOINT "/api/v1/telemetry/ingest"
#define BACKEND_INCIDENT_ENDPOINT  "/api/v1/incidents/report"
#define BACKEND_SCENARIO_ENDPOINT  "/api/v1/test/current-scenario"
#define BACKEND_HEALTH_ENDPOINT    "/"
#define BACKEND_TIMEOUT_MS         4000

// WiFi Configuration for Backend Connection
#define WIFI_SSID                  "A34"           // Change this to your WiFi SSID
#define WIFI_PASSWORD              "01234567"       // Change this to your WiFi password
#define WIFI_MAXIMUM_RETRY         10

static int s_wifi_retry_num = 0;
static bool s_wifi_connected = false;

// Signaled once the first WiFi connection attempt has settled (either got an
// IP, or exhausted its retry budget). face_task's model construction
// (init_face_recognition -> two ESP-DL models) is internal-RAM-hungry for a
// few hundred ms and was observed racing esp_wifi_connect()'s auth-phase
// timer allocation for the last ~15KB of internal SRAM headroom, crashing
// with ESP_ERR_NO_MEM in ets_timer_setfn. face_task waits on this (bounded,
// so it still starts even with no WiFi at all) before touching the models.
static EventGroupHandle_t s_wifi_settle_event = NULL;
#define WIFI_SETTLE_BIT BIT0
static bool s_backend_online = false;
static char s_local_ip_str[32] = "0.0.0.0";
static int s_wifi_rssi = -60;
static bool s_wake_word_active = false;

typedef enum {
    TELEMETRY_MSG_VISION = 0,
    TELEMETRY_MSG_STATUS,
    TELEMETRY_MSG_INCIDENT,
    TELEMETRY_MSG_FETCH_INFO
} telemetry_msg_type_t;

typedef struct {
    telemetry_msg_type_t type;
    char label[32];
    float confidence;
    bool is_threat;
    int owner_id;
    char incident_type[24];
    char incident_message[128];
    char incident_severity[16];
} telemetry_msg_t;

static QueueHandle_t s_telemetry_queue = NULL;

static void queue_vision_detection(const char *label, float confidence, bool is_threat, int owner_id)
{
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_VISION;
    if (label) snprintf(msg.label, sizeof(msg.label), "%s", label);
    msg.confidence = confidence;
    msg.is_threat = is_threat;
    msg.owner_id = owner_id;
    xQueueSend(s_telemetry_queue, &msg, 0);
}

static void queue_incident_report(const char *type, const char *message, const char *severity)
{
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_INCIDENT;
    if (type) snprintf(msg.incident_type, sizeof(msg.incident_type), "%s", type);
    if (message) snprintf(msg.incident_message, sizeof(msg.incident_message), "%s", message);
    if (severity) snprintf(msg.incident_severity, sizeof(msg.incident_severity), "%s", severity);
    xQueueSend(s_telemetry_queue, &msg, 0);
}

static void queue_status_telemetry(void)
{
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_STATUS;
    xQueueSend(s_telemetry_queue, &msg, 0);
}

static void queue_fetch_backend_info(void)
{
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_FETCH_INFO;
    xQueueSend(s_telemetry_queue, &msg, 0);
}

typedef enum {
    SECURITY_UNKNOWN = 0,
    SECURITY_OWNER,
    SECURITY_INTRUDER,
    SECURITY_NO_FACE
} security_state_t;

typedef enum {
    OBJECT_STATE_NONE = 0,
    OBJECT_STATE_NON_DANGEROUS,
    OBJECT_STATE_DANGEROUS
} object_state_t;

static volatile security_state_t g_security_state = SECURITY_NO_FACE;

static volatile int g_owner_id = -1;
static volatile bool g_owner_enrolled = false;
static volatile bool g_enrollment_in_progress = false;

static volatile int g_owner_match_streak = 0;
static volatile int g_intruder_match_streak = 0;

static volatile object_state_t g_object_state = OBJECT_STATE_NONE;

static char g_object_label[32] = {0};
static volatile float g_object_confidence = 0.0f;

static bool g_last_alert_state = false;

static SemaphoreHandle_t s_camera_mutex = NULL;

static uint8_t *s_rgb_full = NULL;
static uint8_t *s_model_input = NULL;

static uint16_t *s_crop_x_lut = NULL;
static uint16_t *s_crop_y_lut = NULL;

static httpd_handle_t stream_httpd = NULL;

static HumanFaceDetect *s_face_detect = nullptr;
static HumanFaceRecognizer *s_face_recognizer = nullptr;

static esp_err_t init_camera();
static camera_fb_t *camera_get_frame();
static void camera_return_frame(camera_fb_t *fb);
static void run_object_detection();
static void update_robot_security_state();

static void log_heap_checkpoint(const char *label)
{
    ESP_LOGW(TAG, "[HEAP] %-24s free_internal=%u largest_internal=%u free_total=%u free_psram=%u",
             label,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static esp_err_t init_camera()
{
    camera_config_t config = {};

    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;

    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;

    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;

    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;

    config.xclk_freq_hz = CAMERA_XCLK_HZ;

    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = CAMERA_JPEG_QUALITY;

    config.fb_count = CAMERA_FB_COUNT;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    ESP_LOGI(TAG, "Initializing camera");

    esp_err_t ret = esp_camera_init(&config);

    if (ret != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Camera initialization failed: 0x%x",
            ret
        );

        return ret;
    }

    sensor_t *sensor = esp_camera_sensor_get();

    if (sensor) {
        sensor->set_framesize(
            sensor,
            FRAMESIZE_VGA
        );

        sensor->set_quality(
            sensor,
            CAMERA_JPEG_QUALITY
        );

        sensor->set_hmirror(
            sensor,
            0
        );

        sensor->set_vflip(
            sensor,
            0
        );

        // --- ESP32-S3 EYE Sensor Tuning (OV2640 / OV3660) ---
        // Clean, crisp, high dynamic range imaging
        sensor->set_brightness(sensor, 0);     // 0 = Neutral balanced lighting (prevents overexposed blowouts)
        sensor->set_contrast(sensor, 1);       // 1 = Crisp contrast enhancement
        sensor->set_saturation(sensor, 1);     // 1 = Rich, vibrant natural color reproduction
        sensor->set_sharpness(sensor, 2);      // 2 = Hardware DSP Edge Sharpening (eliminates blurriness)
        sensor->set_denoise(sensor, 1);        // 1 = Enable Hardware Noise Reduction Filter
        sensor->set_dcw(sensor, 1);            // 1 = Enable Downsize Clear Window (anti-aliased downsampling)
        sensor->set_whitebal(sensor, 1);       // 1 = Enable Auto White Balance
        sensor->set_awb_gain(sensor, 1);       // 1 = Enable Auto White Balance Gain
        sensor->set_wb_mode(sensor, 0);        // 0 = Auto WB Mode
        sensor->set_exposure_ctrl(sensor, 1);  // 1 = Enable Auto Exposure Control (AEC)
        sensor->set_aec2(sensor, 1);           // 1 = Enable DSP Auto Exposure (AEC2)
        sensor->set_ae_level(sensor, 0);       // 0 = Balanced exposure target
        sensor->set_gain_ctrl(sensor, 1);      // 1 = Enable Auto Gain Control (AGC)
        sensor->set_gainceiling(sensor, (gainceiling_t)GAINCEILING_4X); // 4X prevents thermal noise amplification
        sensor->set_bpc(sensor, 1);            // 1 = Enable Black Pixel Correction (removes dead pixels)
        sensor->set_wpc(sensor, 1);            // 1 = Enable White Pixel Correction (removes hot noise speckles)
        sensor->set_raw_gma(sensor, 1);        // 1 = Enable Gamma Correction
        sensor->set_lenc(sensor, 1);           // 1 = Enable Lens Vignetting Correction (brightens corners)
    }

    ESP_LOGI(
        TAG,
        "Camera OK: VGA 640x480 (Optimized ISP)"
    );

    return ESP_OK;
}

static camera_fb_t *camera_get_frame()
{
    if (s_camera_mutex) {
        if (xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
            return NULL;
        }
    }

    camera_fb_t *fb = esp_camera_fb_get();

    if (s_camera_mutex) {
        xSemaphoreGive(s_camera_mutex);
    }

    return fb;
}

static void camera_return_frame(camera_fb_t *fb)
{
    if (!fb)
        return;

    if (s_camera_mutex) {
        if (xSemaphoreTake(s_camera_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            esp_camera_fb_return(fb);
            xSemaphoreGive(s_camera_mutex);
            return;
        }
    }

    esp_camera_fb_return(fb);
}

static bool is_dangerous_object(
    const char *label
)
{
    if (!label)
        return false;

    char name[64] = {0};

    size_t len =
        strlen(label);

    if (len >= sizeof(name))
        len = sizeof(name) - 1;

    for (size_t i = 0; i < len; i++) {

        char c = label[i];

        if (
            c >= 'A' &&
            c <= 'Z'
        ) {
            c = c + ('a' - 'A');
        }

        name[i] = c;
    }

    name[len] = '\0';

    if (
        strstr(name, "knife") ||
        strstr(name, "gun") ||
        strstr(name, "pistol") ||
        strstr(name, "rifle") ||
        strstr(name, "weapon") ||
        strstr(name, "firearm") ||
        strstr(name, "sword") ||
        strstr(name, "blade") ||
        strstr(name, "dagger") ||
        strstr(name, "axe") ||
        strstr(name, "hammer") ||
        strstr(name, "scissor") ||
        strstr(name, "lighter") ||
        strstr(name, "blood") ||
        strstr(name, "grenade")
    ) {
        return true;
    }

    return false;
}

// WiFi event handler
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Deliberately NOT calling esp_wifi_connect() here. STA_START fires
        // the instant esp_wifi_start() returns inside init_wifi_and_espnow()
        // — well before swarm_link_init() runs later in app_main() (display +
        // ESP-NOW master init sit in between). If this handler connected
        // immediately, the STA would already be mid-"connecting" by the time
        // swarm_link_init() tries to pin the radio to channel 6, and
        // esp_wifi_set_channel() silently fails ("STA is scanning or
        // connecting... cannot set channel") — exactly what was happening:
        // the very first swarm WAKE broadcast failed with ESP_ERR_ESPNOW_CHAN
        // every time because the radio was still sitting on channel 1. The
        // first connection attempt is now kicked off explicitly from
        // app_main() after swarm_link_init() has already pinned channel 6;
        // every reconnect after that still goes through the disconnect
        // handler below, which already pins the channel before reconnecting.
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        // Keep the swarm ESP-NOW link alive on channel 6 while STA is unassociated
        // (SWARM_ESPNOW_DESIGN.md §2) — no effect once esp_wifi_connect() below re-associates.
        swarm_link_pin_channel();
        if (s_wifi_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_wifi_retry_num++;
            ESP_LOGI(TAG, "Retry connecting to WiFi (%d/%d)...", s_wifi_retry_num, WIFI_MAXIMUM_RETRY);
        } else {
            ESP_LOGW(TAG, "WiFi reconnect limit reached, will retry shortly");
            s_wifi_retry_num = 0;
            if (s_wifi_settle_event) {
                xEventGroupSetBits(s_wifi_settle_event, WIFI_SETTLE_BIT);
            }
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(s_local_ip_str, sizeof(s_local_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connected to WiFi! IP: %s", s_local_ip_str);
        s_wifi_connected = true;
        s_wifi_retry_num = 0;
        if (s_wifi_settle_event) {
            xEventGroupSetBits(s_wifi_settle_event, WIFI_SETTLE_BIT);
        }

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_wifi_rssi = ap_info.rssi;
            ESP_LOGI(TAG, "Wi-Fi AP Connected on Channel: %d (RSSI: %d dBm)", ap_info.primary, ap_info.rssi);
            espnow_master_sync_channel(ap_info.primary);
        }

        // Immediately ping backend and send initial status
        queue_fetch_backend_info();
        queue_status_telemetry();
    }
}

// HTTP event handler for backend communication
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (evt->user_data != NULL && evt->data_len > 0) {
                char *resp_buf = (char *)evt->user_data;
                size_t current_len = strlen(resp_buf);
                if (current_len + evt->data_len < 510) {
                    memcpy(resp_buf + current_len, evt->data, evt->data_len);
                    resp_buf[current_len + evt->data_len] = '\0';
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t backend_http_post(const char *endpoint, const char *json_payload)
{
    if (!s_wifi_connected) {
        ESP_LOGD(TAG, "WiFi not connected, skipping HTTP POST to %s", endpoint);
        return ESP_ERR_INVALID_STATE;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, endpoint);

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.timeout_ms = BACKEND_TIMEOUT_MS;
    config.method = HTTP_METHOD_POST;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for POST %s", endpoint);
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "Backend POST %s OK (HTTP %d)", endpoint, status_code);
            s_backend_online = true;
        } else {
            ESP_LOGW(TAG, "Backend POST %s returned HTTP %d", endpoint, status_code);
        }
    } else {
        ESP_LOGW(TAG, "Backend POST %s failed: %s", endpoint, esp_err_to_name(err));
        s_backend_online = false;
    }

    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t backend_http_get(const char *endpoint, char *out_buf, size_t max_len)
{
    if (!s_wifi_connected) {
        ESP_LOGD(TAG, "WiFi not connected, skipping HTTP GET to %s", endpoint);
        return ESP_ERR_INVALID_STATE;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, endpoint);

    if (out_buf && max_len > 0) {
        out_buf[0] = '\0';
    }

    esp_http_client_config_t config = {};
    config.url = url;
    config.event_handler = http_event_handler;
    config.timeout_ms = BACKEND_TIMEOUT_MS;
    config.method = HTTP_METHOD_GET;
    config.user_data = out_buf;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for GET %s", endpoint);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "Backend GET %s OK (HTTP %d)", endpoint, status_code);
            s_backend_online = true;
        } else {
            ESP_LOGW(TAG, "Backend GET %s returned HTTP %d", endpoint, status_code);
        }
    } else {
        ESP_LOGW(TAG, "Backend GET %s failed: %s", endpoint, esp_err_to_name(err));
        s_backend_online = false;
    }

    esp_http_client_cleanup(client);
    return err;
}

static void send_vision_to_backend(const telemetry_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "kind", "vision_detection");

    cJSON *payload = cJSON_CreateObject();
    if (payload) {
        cJSON_AddStringToObject(payload, "label", msg->label);
        cJSON_AddNumberToObject(payload, "confidence", (double)msg->confidence);
        cJSON_AddBoolToObject(payload, "is_threat", msg->is_threat);
        if (msg->owner_id >= 0) {
            cJSON_AddNumberToObject(payload, "owner_id", msg->owner_id);
        } else {
            cJSON_AddNullToObject(payload, "owner_id");
        }

        cJSON *bbox = cJSON_CreateObject();
        if (bbox) {
            cJSON_AddNumberToObject(bbox, "x", 0.0);
            cJSON_AddNumberToObject(bbox, "y", 0.0);
            cJSON_AddNumberToObject(bbox, "w", 0.0);
            cJSON_AddNumberToObject(bbox, "h", 0.0);
            cJSON_AddItemToObject(payload, "bbox", bbox);
        }
        cJSON_AddItemToObject(root, "payload", payload);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ESP_LOGI(TAG, "Ingesting Vision Telemetry: %s", json_str);
        backend_http_post(BACKEND_TELEMETRY_ENDPOINT, json_str);
        free(json_str);
    }
    cJSON_Delete(root);
}

static void send_status_to_backend(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "kind", "telemetry");

    cJSON *payload = cJSON_CreateObject();
    if (payload) {
        const char *status_str = g_last_alert_state ? "ALERT" : "PATROL";
        cJSON_AddStringToObject(payload, "status", status_str);
        cJSON_AddNumberToObject(payload, "battery_pct", 88);

        cJSON *sys_info = cJSON_CreateObject();
        if (sys_info) {
            cJSON_AddNumberToObject(sys_info, "free_heap", (double)esp_get_free_heap_size());
            cJSON_AddNumberToObject(sys_info, "psram", (double)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
            cJSON_AddItemToObject(payload, "system_info", sys_info);
        }

        cJSON_AddNumberToObject(payload, "wifi_rssi", s_wifi_rssi);
        cJSON_AddStringToObject(payload, "ip_address", s_local_ip_str);

        int64_t uptime_sec = esp_timer_get_time() / 1000000;
        char ts_buf[32];
        snprintf(ts_buf, sizeof(ts_buf), "uptime_%llds", (long long)uptime_sec);
        cJSON_AddStringToObject(payload, "timestamp", ts_buf);

        cJSON_AddBoolToObject(payload, "wake_word_triggered", s_wake_word_active);
        cJSON_AddStringToObject(payload, "wake_word_label", "Hi ESP");

        cJSON *first_aid = cJSON_CreateObject();
        if (first_aid) {
            cJSON_AddBoolToObject(first_aid, "box_attached", true);
            cJSON_AddBoolToObject(first_aid, "delivered", false);
            cJSON_AddItemToObject(payload, "first_aid_status", first_aid);
        }

        cJSON_AddItemToObject(root, "payload", payload);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ESP_LOGI(TAG, "Ingesting Status Telemetry: %s", json_str);
        backend_http_post(BACKEND_TELEMETRY_ENDPOINT, json_str);
        free(json_str);
    }
    cJSON_Delete(root);
}

static void send_incident_to_backend(const telemetry_msg_t *msg)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON_AddStringToObject(root, "bot_id", BOT_ID);
    cJSON_AddStringToObject(root, "type", msg->incident_type[0] ? msg->incident_type : "INTRUDER");
    cJSON_AddStringToObject(root, "message", msg->incident_message[0] ? msg->incident_message : "Security threat detected");
    cJSON_AddStringToObject(root, "severity", msg->incident_severity[0] ? msg->incident_severity : "CRITICAL");

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        ESP_LOGW(TAG, "Reporting Incident to Backend: %s", json_str);
        backend_http_post(BACKEND_INCIDENT_ENDPOINT, json_str);
        free(json_str);
    }
    cJSON_Delete(root);
}

static void fetch_backend_info(void)
{
    char response[512] = {0};
    esp_err_t err = backend_http_get(BACKEND_HEALTH_ENDPOINT, response, sizeof(response));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Backend Health Check: %s", response[0] ? response : "OK");
    }

    char scenario_resp[512] = {0};
    err = backend_http_get(BACKEND_SCENARIO_ENDPOINT, scenario_resp, sizeof(scenario_resp));
    if (err == ESP_OK && scenario_resp[0] != '\0') {
        cJSON *root = cJSON_Parse(scenario_resp);
        if (root) {
            cJSON *scenario_name = cJSON_GetObjectItem(root, "scenario_name");
            if (scenario_name && cJSON_IsString(scenario_name)) {
                ESP_LOGI(TAG, "Active Backend Swarm Scenario: %s", scenario_name->valuestring);
            }
            cJSON_Delete(root);
        }
    }
}

static void telemetry_task(void *arg)
{
    ESP_LOGI(TAG, "Telemetry background task started CPU%d", xPortGetCoreID());
    telemetry_msg_t msg;

    while (true) {
        if (xQueueReceive(s_telemetry_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case TELEMETRY_MSG_VISION:
                    send_vision_to_backend(&msg);
                    break;
                case TELEMETRY_MSG_STATUS:
                    send_status_to_backend();
                    break;
                case TELEMETRY_MSG_INCIDENT:
                    send_incident_to_backend(&msg);
                    break;
                case TELEMETRY_MSG_FETCH_INFO:
                    fetch_backend_info();
                    break;
                default:
                    break;
            }
        }
    }
}

static void update_robot_security_state()
{
    bool alert = false;

    if (g_security_state == SECURITY_INTRUDER) {
        alert = true;
    }

    if (g_object_state == OBJECT_STATE_DANGEROUS) {
        alert = true;
    }

    if (alert == g_last_alert_state) {
        return;
    }

    g_last_alert_state = alert;

    guardian_command_t command = alert ? CMD_ALERT : CMD_NORMAL;

    esp_err_t ret = espnow_master_send_command(command);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ESP-NOW send failed: %s", esp_err_to_name(ret));
    }

    if (alert) {
        char alert_msg[128] = {0};
        char alert_label[48] = {0}; // "INTRUDER+" (9) + g_object_label (up to 31 + NUL)
        if (g_security_state == SECURITY_INTRUDER && g_object_state == OBJECT_STATE_DANGEROUS) {
            snprintf(alert_msg, sizeof(alert_msg), "Intruder detected with weapon: %s (%.1f%%)",
                     g_object_label, g_object_confidence * 100.0f);
            snprintf(alert_label, sizeof(alert_label), "INTRUDER+%s", g_object_label);
            ESP_LOGW(TAG, "SECURITY ALERT: INTRUDER WITH WEAPON (%s)", g_object_label);
        } else if (g_security_state == SECURITY_INTRUDER) {
            snprintf(alert_msg, sizeof(alert_msg), "Unauthorized intruder detected in protected perimeter");
            snprintf(alert_label, sizeof(alert_label), "INTRUDER");
            ESP_LOGW(TAG, "SECURITY ALERT: INTRUDER");
        } else {
            snprintf(alert_msg, sizeof(alert_msg), "Dangerous object detected: %s (%.1f%%)",
                     g_object_label, g_object_confidence * 100.0f);
            snprintf(alert_label, sizeof(alert_label), "%s", g_object_label);
            ESP_LOGW(TAG, "OBJECT ALERT: DANGEROUS | %s | %.1f%%", g_object_label, g_object_confidence * 100.0f);
        }

        // Local on-device screen — this used to be the missing link: the
        // strip/banner only ever reflected siblings reported over the swarm
        // link, never Guardian's own detection, so an alert Guardian itself
        // raised (logged, sent to backend, sent to the swarm) never showed
        // on Guardian's own screen.
        swarm_display_update_self(alert_label);

        // Send incident report to backend
        queue_incident_report("INTRUDER", alert_msg, "CRITICAL");

        // Send vision detection update
        queue_vision_detection(g_object_label[0] ? g_object_label : "Intruder",
                               g_object_confidence > 0 ? (g_object_confidence * 100.0f) : 90.0f,
                               true,
                               g_owner_id);

        // Tell the swarm (Pathfinder/Warden), independent of backend Wi-Fi state
        swarm_send_security_alert(g_security_state == SECURITY_INTRUDER,
                                   g_object_state == OBJECT_STATE_DANGEROUS,
                                   g_object_confidence,
                                   g_object_label[0] ? g_object_label : "Intruder");
    } else {
        ESP_LOGI(TAG, "SYSTEM NORMAL");
        swarm_display_update_self(nullptr);
        queue_vision_detection("Clear", 100.0f, false, g_owner_id);
        swarm_send_all_clear();
    }
}

static bool init_spiffs()
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiflash",
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true
    };

    esp_err_t ret =
        esp_vfs_spiffs_register(
            &conf
        );

    if (ret != ESP_OK) {

        ESP_LOGE(
            TAG,
            "SPIFFS mount failed: 0x%x",
            ret
        );

        return false;
    }

    struct stat st;

    if (
        stat(
            "/spiflash/face_db",
            &st
        ) != 0
    ) {
        mkdir(
            "/spiflash/face_db",
            0777
        );
    }

    return true;
}

static bool init_face_recognition()
{
    log_heap_checkpoint("before HumanFaceDetect()");

    s_face_detect =
        new HumanFaceDetect();

    if (!s_face_detect)
        return false;

    log_heap_checkpoint("after HumanFaceDetect()");

    HumanFaceFeat *feat =
        new HumanFaceFeat();

    if (!feat)
        return false;

    log_heap_checkpoint("after HumanFaceFeat()");

    s_face_recognizer =
        new HumanFaceRecognizer(
            feat,
            (char *)"/spiflash/face_db"
        );

    if (!s_face_recognizer)
        return false;

    log_heap_checkpoint("after HumanFaceRecognizer()");

    ESP_LOGI(
        TAG,
        "Face AI OK"
    );

    log_heap_checkpoint("after face models loaded");

    return true;
}

#define OWNER_NVS_NAMESPACE "aegis_owner"
#define OWNER_NVS_KEY "owner_id"

static bool save_owner_id(
    int owner_id
)
{
    nvs_handle_t handle;

    esp_err_t ret =
        nvs_open(
            OWNER_NVS_NAMESPACE,
            NVS_READWRITE,
            &handle
        );

    if (ret != ESP_OK)
        return false;

    ret =
        nvs_set_i32(
            handle,
            OWNER_NVS_KEY,
            owner_id
        );

    if (ret == ESP_OK)
        ret = nvs_commit(handle);

    nvs_close(handle);

    return ret == ESP_OK;
}

static bool load_owner_id()
{
    nvs_handle_t handle;

    esp_err_t ret =
        nvs_open(
            OWNER_NVS_NAMESPACE,
            NVS_READONLY,
            &handle
        );

    if (ret != ESP_OK) {

        g_owner_id = -1;
        g_owner_enrolled = false;

        return false;
    }

    int32_t owner_id = -1;

    ret =
        nvs_get_i32(
            handle,
            OWNER_NVS_KEY,
            &owner_id
        );

    nvs_close(handle);

    if (
        ret != ESP_OK ||
        owner_id < 0
    ) {

        g_owner_id = -1;
        g_owner_enrolled = false;

        return false;
    }

    g_owner_id =
        (int)owner_id;

    g_owner_enrolled = true;

    ESP_LOGI(
        TAG,
        "Owner loaded: ID=%d",
        g_owner_id
    );

    return true;
}

static bool enroll_owner(
    uint8_t *rgb,
    std::list<dl::detect::result_t> &faces
)
{
    if (
        !rgb ||
        !s_face_recognizer ||
        faces.empty()
    ) {
        return false;
    }

    dl::image::img_t img = {
        .data = rgb,
        .width = CAM_WIDTH,
        .height = CAM_HEIGHT,
        .pix_type =
            dl::image::DL_IMAGE_PIX_TYPE_RGB888
    };

    int new_owner_id =
        s_face_recognizer->get_num_feats();

    esp_err_t ret =
        s_face_recognizer->enroll(
            img,
            faces
        );

    if (ret != ESP_OK)
        return false;

    g_owner_id =
        new_owner_id;

    g_owner_enrolled = true;

    if (
        !save_owner_id(
            g_owner_id
        )
    ) {

        g_owner_id = -1;
        g_owner_enrolled = false;

        return false;
    }

    ESP_LOGI(
        TAG,
        "OWNER ENROLLED ID=%d",
        g_owner_id
    );

    return true;
}

static void face_task(
    void *arg
)
{
    ESP_LOGI(
        TAG,
        "Face task started CPU%d",
        xPortGetCoreID()
    );

    // Hold off loading the ESP-DL models (internal-RAM-hungry for a few
    // hundred ms) until the first WiFi connection attempt has settled, so
    // model construction doesn't race esp_wifi_connect()'s auth-phase timer
    // allocation for the last scraps of internal SRAM. Bounded wait — still
    // proceeds even if WiFi never comes up at all.
    if (s_wifi_settle_event) {
        xEventGroupWaitBits(
            s_wifi_settle_event,
            WIFI_SETTLE_BIT,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(6000)
        );

        // Getting an IP doesn't mean WiFi has gone quiet: the first outbound
        // traffic right after connecting (ESP-NOW channel sync, the initial
        // backend HTTP calls) triggers its own internal-RAM-hungry setup —
        // observed concretely as an AMPDU block-ack session negotiation
        // (ieee80211_ampdu_request) needing a coex timer at the exact moment
        // model loading below started, same ESP_ERR_NO_MEM/ets_timer_setfn
        // failure as the auth-phase race this event group was built for.
        // Give that initial post-connect burst a little room to finish.
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    if (
        !init_face_recognition()
    ) {
        ESP_LOGE(
            TAG,
            "Face AI unavailable"
        );

        vTaskDelete(NULL);
        return;
    }

    load_owner_id();

    if (g_owner_enrolled) {

        int db_count =
            s_face_recognizer
                ->get_num_feats();

        if (
            db_count <=
            g_owner_id
        ) {

            g_owner_id = -1;
            g_owner_enrolled = false;

            ESP_LOGW(
                TAG,
                "Owner database invalid"
            );
        }
    }

    size_t rgb_size =
        CAM_WIDTH *
        CAM_HEIGHT *
        3;

    uint8_t *rgb =
        (uint8_t *)
        heap_caps_malloc(
            rgb_size,
            MALLOC_CAP_SPIRAM |
            MALLOC_CAP_8BIT
        );

    if (!rgb) {

        ESP_LOGE(
            TAG,
            "Face RGB buffer failed"
        );

        vTaskDelete(NULL);
        return;
    }

    TickType_t last =
        xTaskGetTickCount();

    while (true) {

        camera_fb_t *fb =
            camera_get_frame();

        if (!fb) {

            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

        bool ok =
            fmt2rgb888(
                fb->buf,
                fb->len,
                fb->format,
                rgb
            );

        camera_return_frame(fb);

        if (!ok) {

            vTaskDelay(
                pdMS_TO_TICKS(100)
            );

            continue;
        }

        dl::image::img_t img = {
            .data = rgb,
            .width = CAM_WIDTH,
            .height = CAM_HEIGHT,
            .pix_type =
                dl::image::DL_IMAGE_PIX_TYPE_RGB888
        };

        auto faces =
            s_face_detect->run(img);

        if (faces.empty()) {

            bool was_face = (g_security_state != SECURITY_NO_FACE);

            g_security_state =
                SECURITY_NO_FACE;

            g_owner_match_streak = 0;
            g_intruder_match_streak = 0;

            g_object_state =
                OBJECT_STATE_NONE;

            g_object_label[0] = '\0';

            g_object_confidence = 0.0f;

            update_robot_security_state();

            if (was_face) {
                queue_vision_detection("Clear", 100.0f, false, -1);
            }

            vTaskDelayUntil(
                &last,
                pdMS_TO_TICKS(
                    FACE_INTERVAL_MS
                )
            );

            continue;
        }

#if AUTO_ENROLL_FIRST_FACE

        if (
            !g_owner_enrolled &&
            !g_enrollment_in_progress
        ) {

            g_enrollment_in_progress =
                true;

            ESP_LOGI(
                TAG,
                "First face detected -> enrolling owner"
            );

            bool enrolled =
                enroll_owner(
                    rgb,
                    faces
                );

            if (enrolled) {

                g_security_state =
                    SECURITY_OWNER;

                g_owner_match_streak =
                    OWNER_CONFIRM_COUNT;

                queue_vision_detection("Owner", 100.0f, false, g_owner_id);

            } else {

                g_security_state =
                    SECURITY_UNKNOWN;
            }

            g_enrollment_in_progress =
                false;

        } else

#endif

        {

            auto result =
                s_face_recognizer
                    ->recognize(
                        img,
                        faces
                    );

            if (!result.empty()) {

                float sim =
                    result[0].similarity;

                int id =
                    result[0].id;

                ESP_LOGI(
                    TAG,
                    "Face ID=%d similarity=%.3f",
                    id,
                    sim
                );

                if (
                    g_owner_enrolled &&
                    id == g_owner_id &&
                    sim >=
                    OWNER_SIMILARITY_THRESHOLD
                ) {

                    g_owner_match_streak++;

                    g_intruder_match_streak = 0;

                    if (
                        g_owner_match_streak >=
                        OWNER_CONFIRM_COUNT
                    ) {

                        g_security_state =
                            SECURITY_OWNER;

                        ESP_LOGI(
                            TAG,
                            "OWNER CONFIRMED"
                        );

                        queue_vision_detection("Owner", sim * 100.0f, false, g_owner_id);
                    }

                } else {

                    g_intruder_match_streak++;

                    g_owner_match_streak = 0;

                    if (
                        g_intruder_match_streak >=
                        INTRUDER_CONFIRM_COUNT
                    ) {

                        g_security_state =
                            SECURITY_INTRUDER;

                        ESP_LOGW(
                            TAG,
                            "INTRUDER CONFIRMED"
                        );

                        queue_vision_detection("Intruder", (1.0f - sim) * 100.0f, true, -1);
                    }
                }

            } else {

                g_owner_match_streak = 0;

                g_intruder_match_streak++;

                if (
                    g_intruder_match_streak >=
                    INTRUDER_CONFIRM_COUNT
                ) {

                    g_security_state =
                        SECURITY_INTRUDER;

                    ESP_LOGW(
                        TAG,
                        "UNKNOWN FACE -> INTRUDER"
                    );

                    queue_vision_detection("Intruder", 90.0f, true, -1);
                }
            }
        }

        update_robot_security_state();

        vTaskDelayUntil(
            &last,
            pdMS_TO_TICKS(
                FACE_INTERVAL_MS
            )
        );
    }
}

static int raw_feature_get_data(
    size_t offset,
    size_t length,
    float *out_ptr
)
{
    if (
        !s_model_input ||
        !out_ptr
    )
        return -1;

    size_t pixel =
        offset * 3;

    for (
        size_t i = 0;
        i < length;
        i++
    ) {

        out_ptr[i] =
            (float)(
                ((uint32_t)
                    s_model_input[pixel]
                    << 16) |
                ((uint32_t)
                    s_model_input[pixel + 1]
                    << 8) |
                (uint32_t)
                    s_model_input[pixel + 2]
            );

        pixel += 3;
    }

    return 0;
}

static void build_crop_resize_luts(
    int sw,
    int sh,
    int dw,
    int dh
)
{
    float source_aspect =
        (float)sw /
        (float)sh;

    float destination_aspect =
        (float)dw /
        (float)dh;

    int crop_width = sw;
    int crop_height = sh;

    if (
        source_aspect >
        destination_aspect
    ) {

        crop_width =
            (int)(
                sh *
                destination_aspect
            );

    } else {

        crop_height =
            (int)(
                sw /
                destination_aspect
            );
    }

    int offset_x =
        (sw - crop_width) / 2;

    int offset_y =
        (sh - crop_height) / 2;

    s_crop_x_lut =
        (uint16_t *)
        malloc(
            sizeof(uint16_t) * dw
        );

    s_crop_y_lut =
        (uint16_t *)
        malloc(
            sizeof(uint16_t) * dh
        );

    if (
        !s_crop_x_lut ||
        !s_crop_y_lut
    )
        return;

    for (
        int x = 0;
        x < dw;
        x++
    ) {

        int sx =
            offset_x +
            (x * crop_width / dw);

        if (sx >= sw)
            sx = sw - 1;

        s_crop_x_lut[x] =
            (uint16_t)sx;
    }

    for (
        int y = 0;
        y < dh;
        y++
    ) {

        int sy =
            offset_y +
            (y * crop_height / dh);

        if (sy >= sh)
            sy = sh - 1;

        s_crop_y_lut[y] =
            (uint16_t)sy;
    }
}

static void fast_crop_resize(
    const uint8_t *src,
    int sw,
    uint8_t *dst,
    int dw,
    int dh
)
{
    if (
        !src ||
        !dst ||
        !s_crop_x_lut ||
        !s_crop_y_lut
    )
        return;

    for (
        int y = 0;
        y < dh;
        y++
    ) {

        uint16_t sy =
            s_crop_y_lut[y];

        const uint8_t *src_row =
            src +
            (size_t)sy *
            sw *
            3;

        uint8_t *dst_row =
            dst +
            (size_t)y *
            dw *
            3;

        for (
            int x = 0;
            x < dw;
            x++
        ) {

            uint16_t sx =
                s_crop_x_lut[x];

            size_t src_idx =
                (size_t)sx * 3;

            size_t dst_idx =
                (size_t)x * 3;

            dst_row[dst_idx] =
                src_row[src_idx];

            dst_row[dst_idx + 1] =
                src_row[src_idx + 1];

            dst_row[dst_idx + 2] =
                src_row[src_idx + 2];
        }
    }
}

static void run_object_detection()
{
    if (
        g_security_state !=
        SECURITY_OWNER &&
        g_security_state !=
        SECURITY_INTRUDER
    ) {
        return;
    }

    if (
        !s_rgb_full ||
        !s_model_input
    )
        return;

    camera_fb_t *fb =
        camera_get_frame();

    if (!fb)
        return;

    bool ok =
        fmt2rgb888(
            fb->buf,
            fb->len,
            fb->format,
            s_rgb_full
        );

    camera_return_frame(fb);

    if (!ok)
        return;

    fast_crop_resize(
        s_rgb_full,
        CAM_WIDTH,
        s_model_input,
        EI_CLASSIFIER_INPUT_WIDTH,
        EI_CLASSIFIER_INPUT_HEIGHT
    );

    signal_t signal = {
        .get_data =
            raw_feature_get_data,

        .total_length =
            (size_t)
            EI_CLASSIFIER_INPUT_WIDTH *
            (size_t)
            EI_CLASSIFIER_INPUT_HEIGHT
    };

    ei_impulse_result_t result = {};

    EI_IMPULSE_ERROR ret =
        run_classifier(
            &signal,
            &result,
            false
        );

    if (
        ret !=
        EI_IMPULSE_OK
    ) {

        ESP_LOGW(
            TAG,
            "Edge Impulse error: %d",
            ret
        );

        return;
    }

    bool dangerous_found = false;

    float best_confidence = 0.0f;

    char best_label[32] = {0};

    for (
        size_t i = 0;
        i < result.bounding_boxes_count;
        i++
    ) {

        auto &b =
            result.bounding_boxes[i];

        if (
            b.value <
            OBJECT_THRESHOLD
        )
            continue;

        bool dangerous =
            is_dangerous_object(
                b.label
            );

        ESP_LOGI(
            TAG,
            "OBJECT: %s %.1f%%",
            b.label,
            b.value * 100.0f
        );

        if (
            dangerous &&
            b.value >
            best_confidence
        ) {

            dangerous_found = true;

            best_confidence =
                b.value;

            snprintf(
                best_label,
                sizeof(best_label),
                "%s",
                b.label
            );
        }
    }

    if (dangerous_found) {

        g_object_state =
            OBJECT_STATE_DANGEROUS;

        g_object_confidence =
            best_confidence;

        snprintf(
            g_object_label,
            sizeof(g_object_label),
            "%s",
            best_label
        );

        ESP_LOGW(
            TAG,
            "================================"
        );

        ESP_LOGW(
            TAG,
            "DANGEROUS OBJECT"
        );

        ESP_LOGW(
            TAG,
            "Object: %s",
            g_object_label
        );

        ESP_LOGW(
            TAG,
            "Confidence: %.1f%%",
            g_object_confidence * 100.0f
        );

        ESP_LOGW(
            TAG,
            "================================"
        );

        queue_vision_detection(g_object_label, g_object_confidence * 100.0f, true, g_owner_id);

    } else {

        bool any_object = false;

        const char *normal_label =
            NULL;

        float normal_confidence =
            0.0f;

        for (
            size_t i = 0;
            i < result.bounding_boxes_count;
            i++
        ) {

            auto &b =
                result.bounding_boxes[i];

            if (
                b.value >=
                OBJECT_THRESHOLD
            ) {

                any_object = true;

                if (
                    b.value >
                    normal_confidence
                ) {

                    normal_confidence =
                        b.value;

                    normal_label =
                        b.label;
                }
            }
        }

        if (any_object) {

            g_object_state =
                OBJECT_STATE_NON_DANGEROUS;

            g_object_confidence =
                normal_confidence;

            if (normal_label) {

                snprintf(
                    g_object_label,
                    sizeof(g_object_label),
                    "%s",
                    normal_label
                );

            } else {

                snprintf(
                    g_object_label,
                    sizeof(g_object_label),
                    "object"
                );
            }

            ESP_LOGI(
                TAG,
                "================================"
            );

            ESP_LOGI(
                TAG,
                "NON-DANGEROUS OBJECT"
            );

            ESP_LOGI(
                TAG,
                "Object: %s",
                g_object_label
            );

            ESP_LOGI(
                TAG,
                "Confidence: %.1f%%",
                g_object_confidence * 100.0f
            );

            ESP_LOGI(
                TAG,
                "================================"
            );

            queue_vision_detection(g_object_label, g_object_confidence * 100.0f, false, g_owner_id);

        } else {

            g_object_state =
                OBJECT_STATE_NONE;

            g_object_label[0] =
                '\0';

            g_object_confidence =
                0.0f;

            ESP_LOGI(
                TAG,
                "No object detected"
            );
        }
    }

    update_robot_security_state();
}

static void ai_task(
    void *arg
)
{
    ESP_LOGI(
        TAG,
        "Object AI task CPU%d",
        xPortGetCoreID()
    );

    size_t full_size =
        CAM_WIDTH *
        CAM_HEIGHT *
        3;

    size_t model_size =
        EI_CLASSIFIER_INPUT_WIDTH *
        EI_CLASSIFIER_INPUT_HEIGHT *
        3;

    s_rgb_full =
        (uint8_t *)
        heap_caps_malloc(
            full_size,
            MALLOC_CAP_SPIRAM |
            MALLOC_CAP_8BIT
        );

    s_model_input =
        (uint8_t *)
        heap_caps_aligned_alloc(
            BUF_ALIGN,
            model_size,
            MALLOC_CAP_SPIRAM |
            MALLOC_CAP_8BIT
        );

    build_crop_resize_luts(
        CAM_WIDTH,
        CAM_HEIGHT,
        EI_CLASSIFIER_INPUT_WIDTH,
        EI_CLASSIFIER_INPUT_HEIGHT
    );

    if (
        !s_rgb_full ||
        !s_model_input ||
        !s_crop_x_lut ||
        !s_crop_y_lut
    ) {

        ESP_LOGE(
            TAG,
            "Object AI buffer allocation failed"
        );

        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(
        TAG,
        "Object model input: %ux%u",
        EI_CLASSIFIER_INPUT_WIDTH,
        EI_CLASSIFIER_INPUT_HEIGHT
    );

    TickType_t last =
        xTaskGetTickCount();

    while (true) {

        if (
            g_security_state ==
                SECURITY_OWNER ||
            g_security_state ==
                SECURITY_INTRUDER
        ) {

            run_object_detection();
        }

        vTaskDelayUntil(
            &last,
            pdMS_TO_TICKS(
                OBJECT_INTERVAL_MS
            )
        );
    }
}

static esp_err_t stream_handler(
    httpd_req_t *req
)
{
    static const char *content_type =
        "multipart/x-mixed-replace;boundary=frame";

    static const char *boundary =
        "\r\n--frame\r\n";

    static const char *part =
        "Content-Type: image/jpeg\r\n"
        "Content-Length: %u\r\n\r\n";

    esp_err_t ret =
        httpd_resp_set_type(
            req,
            content_type
        );

    if (ret != ESP_OK)
        return ret;

    char header[64];

    while (true) {

        camera_fb_t *fb =
            camera_get_frame();

        if (!fb) {

            vTaskDelay(
                pdMS_TO_TICKS(50)
            );

            continue;
        }

        int header_len =
            snprintf(
                header,
                sizeof(header),
                part,
                (unsigned)fb->len
            );

        ret =
            httpd_resp_send_chunk(
                req,
                boundary,
                strlen(boundary)
            );

        if (ret == ESP_OK) {

            ret =
                httpd_resp_send_chunk(
                    req,
                    header,
                    header_len
                );
        }

        if (ret == ESP_OK) {

            ret =
                httpd_resp_send_chunk(
                    req,
                    (const char *)fb->buf,
                    fb->len
                );
        }

        camera_return_frame(fb);

        if (ret != ESP_OK)
            return ret;

        vTaskDelay(
            pdMS_TO_TICKS(
                STREAM_DELAY_MS
            )
        );
    }
}

static esp_err_t root_handler(
    httpd_req_t *req
)
{
    httpd_resp_set_status(
        req,
        "302 Found"
    );

    httpd_resp_set_hdr(
        req,
        "Location",
        "/snapshot"
    );

    return httpd_resp_send(
        req,
        NULL,
        0
    );
}

static esp_err_t snapshot_handler(
    httpd_req_t *req
)
{
    camera_fb_t *fb =
        camera_get_frame();

    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(
        req,
        "image/jpeg"
    );

    httpd_resp_set_hdr(
        req,
        "Content-Disposition",
        "inline; filename=snapshot.jpg"
    );

    httpd_resp_set_hdr(
        req,
        "Access-Control-Allow-Origin",
        "*"
    );

    esp_err_t ret =
        httpd_resp_send(
            req,
            (const char *)fb->buf,
            fb->len
        );

    camera_return_frame(fb);

    return ret;
}

static void start_stream_server()
{
    httpd_config_t config =
        HTTPD_DEFAULT_CONFIG();

    config.stack_size = 8192;

    config.max_uri_handlers = 8;

    // CONFIG_LWIP_MAX_SOCKETS is the shared ceiling for the whole system —
    // this server AND every outbound esp_http_client call (backend
    // telemetry) draw sockets from the same pool. Left at the
    // HTTPD_DEFAULT_CONFIG() default (7), a viewer polling /snapshot could
    // claim most of that pool, starving telemetry's outbound POSTs of a
    // socket entirely ("Failed to create socket") while /snapshot itself
    // kept working — observed exactly this way. Capping this server's own
    // share leaves headroom for the client side, and lru_purge_enable
    // reclaims a stale/idle viewer connection instead of just refusing new
    // ones once that cap is hit.
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_handler,
        .user_ctx = NULL
    };

    // Live MJPEG /stream is intentionally not registered — snapshot-only,
    // same reasoning as Warden (see warden_camera_stream.cpp): a live stream
    // holds up the camera indefinitely for as long as a viewer is connected,
    // instead of the ~single-frame pause a snapshot costs.

    httpd_uri_t snapshot = {
        .uri = "/snapshot",
        .method = HTTP_GET,
        .handler = snapshot_handler,
        .user_ctx = NULL
    };

    esp_err_t ret =
        httpd_start(
            &stream_httpd,
            &config
        );

    if (ret != ESP_OK) {

        ESP_LOGE(
            TAG,
            "HTTP server failed: 0x%x",
            ret
        );

        return;
    }

    httpd_register_uri_handler(
        stream_httpd,
        &root
    );

    httpd_register_uri_handler(
        stream_httpd,
        &snapshot
    );

    ESP_LOGI(
        TAG,
        "HTTP server active (snapshot-only, no live /stream)"
    );
}

static const char *
security_state_string(
    security_state_t state
)
{
    switch (state) {

        case SECURITY_OWNER:
            return "OWNER";

        case SECURITY_INTRUDER:
            return "INTRUDER";

        case SECURITY_NO_FACE:
            return "NO FACE";

        default:
            return "UNKNOWN";
    }
}

static const char *
object_state_string(
    object_state_t state
)
{
    switch (state) {

        case OBJECT_STATE_DANGEROUS:
            return "DANGEROUS";

        case OBJECT_STATE_NON_DANGEROUS:
            return "NON-DANGEROUS";

        default:
            return "NONE";
    }
}

static void status_task(
    void *arg
)
{
    while (true) {

        ESP_LOGI(
            TAG,
            "Security=%s | OwnerID=%d | Enrolled=%s | Object=%s | Label=%s | Confidence=%.1f%% | WiFi=%s (%s)",
            security_state_string(
                g_security_state
            ),
            g_owner_id,
            g_owner_enrolled
                ? "YES"
                : "NO",
            object_state_string(
                g_object_state
            ),
            g_object_label[0]
                ? g_object_label
                : "NONE",
            g_object_confidence * 100.0f,
            s_wifi_connected ? "CONNECTED" : "CONNECTING...",
            s_local_ip_str
        );

        if (s_wifi_connected) {
            queue_status_telemetry();
        }

        vTaskDelay(
            pdMS_TO_TICKS(5000)
        );
    }
}

static bool init_wifi_and_espnow()
{
    esp_err_t ret;

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event_loop_create failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_netif_t *sta = esp_netif_create_default_wifi_sta();
    if (!sta) {
        ESP_LOGE(TAG, "Failed to create default wifi sta");
        return false;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_INIT_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Register event handlers for Wi-Fi and IP events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode STA failed: %s", esp_err_to_name(ret));
        return false;
    }

    wifi_config_t wifi_config = {};
    snprintf((char *)wifi_config.sta.ssid, sizeof(wifi_config.sta.ssid), "%s", WIFI_SSID);
    snprintf((char *)wifi_config.sta.password, sizeof(wifi_config.sta.password), "%s", WIFI_PASSWORD);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_wifi_set_ps(WIFI_PS_NONE);

    return true;
}

// ── Swarm: hazard response (Warden fire/smoke/gas -> Guardian searches for
// the owner) ────────────────────────────────────────────────────────────
// Guardian has no dedicated "go find a person" navigation — what it actually
// has is face_task, which already runs continuously and is how it would
// ever encounter the owner in the first place. So this doesn't add new
// navigation; it makes sure a hazard alert can't leave the Slave sitting
// stopped (from an unrelated earlier security CMD_ALERT) while face_task
// keeps scanning as it roams. That's the honest scope of "search" here.
static volatile bool g_owner_search_mode = false;

static void on_swarm_hazard_alert(uint8_t hazard_bits) {
    (void)hazard_bits;
    if (!g_owner_search_mode) {
        ESP_LOGW(TAG, "Swarm: Warden hazard alert -> Guardian entering OWNER SEARCH mode");
        g_owner_search_mode = true;
    }
    // Make sure movement isn't blocked by an unrelated earlier alert-stop.
    espnow_master_send_command(CMD_NORMAL);
}

static void on_swarm_all_clear(void) {
    if (g_owner_search_mode) {
        ESP_LOGI(TAG, "Swarm: ALL_CLEAR received -> Guardian resuming normal patrol");
        g_owner_search_mode = false;
    }
}

extern "C" void app_main()
{
    ESP_LOGI(
        TAG,
        "======================================"
    );

    ESP_LOGI(
        TAG,
        "AEGIS GUARDIAN MASTER START"
    );

    ESP_LOGI(
        TAG,
        "======================================"
    );

    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 240,
        .light_sleep_enable = false
    };

    esp_pm_configure(
        &pm_config
    );

    esp_err_t ret =
        nvs_flash_init();

    if (
        ret ==
        ESP_ERR_NVS_NO_FREE_PAGES ||
        ret ==
        ESP_ERR_NVS_NEW_VERSION_FOUND
    ) {

        ESP_ERROR_CHECK(
            nvs_flash_erase()
        );

        ret =
            nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

#if ENABLE_VOICE_ACTIVATION

    if (
        voice_activation_init()
        != ESP_OK
    ) {

        ESP_LOGE(
            TAG,
            "VOICE INIT FAILED"
        );

        return;
    }

    voice_activation_start();

    ESP_LOGI(
        TAG,
        "WAITING FOR: Hi ESP"
    );

    while (
        !voice_activation_wait(
            portMAX_DELAY
        )
    ) {
        vTaskDelay(
            pdMS_TO_TICKS(10)
        );
    }

    ESP_LOGI(
        TAG,
        "Hi ESP -> AEGIS ACTIVATED"
    );

#endif

    s_camera_mutex =
        xSemaphoreCreateMutex();

    if (!s_camera_mutex) {

        ESP_LOGE(
            TAG,
            "Camera mutex failed"
        );

        return;
    }

    if (
        init_camera()
        != ESP_OK
    )
        return;

    log_heap_checkpoint("after init_camera");

    if (
        !init_spiffs()
    )
        return;

    // Create telemetry message queue
    s_telemetry_queue = xQueueCreate(20, sizeof(telemetry_msg_t));
    if (!s_telemetry_queue) {
        ESP_LOGE(TAG, "Telemetry queue creation failed");
    }

    // Start background telemetry dispatcher task
    xTaskCreatePinnedToCore(
        telemetry_task,
        "telemetry_task",
        8192,
        NULL,
        2,
        NULL,
        0
    );

    s_wifi_settle_event = xEventGroupCreate();

    if (
        !init_wifi_and_espnow()
    )
        return;

    log_heap_checkpoint("after wifi_init/start");

    ret =
        espnow_master_init();

    if (
        ret != ESP_OK
    ) {

        ESP_LOGE(
            TAG,
            "ESP-NOW init failed: %s",
            esp_err_to_name(ret)
        );

        return;
    }

    log_heap_checkpoint("after espnow_master_init");

    // Swarm on-device status display (SWARM_ESPNOW_DESIGN.md §11) — bring up
    // the integrated LCD/LVGL stack before the swarm link, so swarm_link can
    // push updates to it as soon as it starts hearing from siblings.
    //
    // Buffer + LVGL task stack both live in PSRAM (same fix applied to
    // Warden, same reasoning): keeping them in internal SRAM avoided a visual
    // tearing issue confirmed on Pathfinder specifically, but on Warden it
    // turned out to cost enough internal SRAM that the fire/smoke AI task's
    // own stack allocation started silently failing — face_task/ai_task below
    // have the exact same unchecked-allocation shape (see the return-value
    // check now added there). Guardian's whole purpose is that detection
    // running, so it takes priority over a tearing risk that was never even
    // confirmed on this board specifically — revisit if it shows up here.
    bsp_display_cfg_t disp_cfg = {
        .lvgl_port_cfg = {
            .task_priority = 4,
            .task_stack = 7168,
            .task_affinity = -1,
            .task_max_sleep_ms = 500,
            .task_stack_caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
            .timer_period_ms = 5,
        },
        // Was *40 rows (19.2KB/flush @16bpp). Internal RAM is down to a
        // ~1.8KB largest-contiguous-block margin once face_task's models are
        // loaded (see the "[HEAP] after face models loaded" checkpoint), and
        // an SPI DMA priv-buffer allocation for a flush this size was
        // observed failing outright ("Failed to allocate priv TX buffer" /
        // "io tx color failed") — the display silently stopped updating.
        // Shrinking the flush chunk alone didn't fix it — the identical
        // error recurred at the same *10 size, because the shrink only
        // changes how much data LVGL hands to one flush, not why the SPI
        // driver needs a scratch allocation at all: a PSRAM-resident draw
        // buffer isn't natively DMA-reachable by this SPI peripheral, so
        // spi_master bounce-copies through a temporary internal buffer on
        // every flush (setup_dma_priv_buffer) — and that's the allocation
        // failing under memory pressure. Buffer is small now (10 rows), so
        // keeping it in internal RAM instead costs only a few KB permanently
        // and makes it natively DMA-capable, removing the bounce-copy (and
        // its failure mode) entirely rather than just shrinking it.
        .buffer_size = BSP_LCD_H_RES * 10,
        .double_buffer = false,
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }
    };
    lv_display_t *swarm_disp = bsp_display_start_with_config(&disp_cfg);
    if (swarm_disp != nullptr) {
        // Physical panel is mounted upside-down relative to how the BSP
        // assumes it's oriented by default.
        bsp_display_rotate(swarm_disp, LV_DISPLAY_ROTATION_180);
        // The BSP configures the backlight PWM channel at 0% duty (off) and
        // never turns it on itself — without this call, LVGL renders fine but
        // nothing is ever visible.
        bsp_display_backlight_on();
        swarm_display_init();
    } else {
        ESP_LOGW(TAG, "bsp_display_start_with_config() failed; swarm status will not be shown on-screen");
    }

    log_heap_checkpoint("after display init");

    // Swarm ESP-NOW link (Master-to-Master, independent of the Master<->Slave
    // link above) — see SWARM_ESPNOW_DESIGN.md. Sending WAKE here, right after
    // the swarm radio comes up, is the earliest point it can go out; the wake
    // word itself was already detected earlier in this function.
    if (swarm_link_init() == ESP_OK) {
        swarm_send_wake();
        swarm_link_set_hazard_alert_cb(on_swarm_hazard_alert);
        swarm_link_set_all_clear_cb(on_swarm_all_clear);
    }

    log_heap_checkpoint("after swarm_link_init");

    // Only now start the actual backend Wi-Fi connection attempt — after the
    // swarm channel-6 pin above has already landed. See the comment on
    // WIFI_EVENT_STA_START in wifi_event_handler() for why the ordering here
    // matters.
    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s ...", WIFI_SSID);
    log_heap_checkpoint("just before esp_wifi_connect");
    esp_wifi_connect();

    start_stream_server();

    // face_task's stack itself (reserved right here, at creation time — not
    // when the task body first runs) must come from internal RAM: it reads
    // the face database off SPIFFS via HumanFaceRecognizer's DataBase
    // constructor, and ESP-IDF requires an internal-RAM stack for any task
    // doing raw flash I/O (a PSRAM-resident stack is unreachable while flash
    // reads disable the cache). A fixed 300ms delay here wasn't enough —
    // esp_wifi_connect()'s auth/assoc burst was still consuming internal RAM
    // at that point, leaving only ~12KB contiguous free for a 16KB stack
    // request, so xTaskCreatePinnedToCore silently — well, loudly, see the
    // check below — failed and face detection never ran. Wait for the same
    // WiFi-settle signal face_task itself waits on, but here, so the STACK
    // RESERVATION happens after the connect burst subsides, not just the
    // model loading inside the task body.
    if (s_wifi_settle_event) {
        xEventGroupWaitBits(
            s_wifi_settle_event,
            WIFI_SETTLE_BIT,
            pdFALSE,
            pdTRUE,
            pdMS_TO_TICKS(6000)
        );
    }

    log_heap_checkpoint("before face_task creation");

    // Return values were previously unchecked here — a creation failure
    // (e.g. not enough free internal SRAM for the 16KB stack) was completely
    // silent: no log, no crash, the task just never existed. That's the
    // exact bug found on Warden's fire/smoke task; check for it here too
    // instead of guessing whether detection is actually running.
    BaseType_t face_task_result = xTaskCreatePinnedToCore(
        face_task,
        "face_task",
        16 * 1024,
        NULL,
        5,
        NULL,
        0
    );
    if (face_task_result != pdPASS) {
        ESP_LOGE(TAG, "FAILED to create face_task (result=%d) — intruder/owner detection will NOT run! Free heap: %u, largest free INTERNAL block: %u",
                 (int)face_task_result, (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    vTaskDelay(
        pdMS_TO_TICKS(500)
    );

    // Internal SRAM is already down to ~3KB free by this point (face_task's
    // 16KB stack plus Wi-Fi's driver task/buffers ahead of it eat the rest),
    // which silently failed this allocation every time. Stack in PSRAM
    // instead — same fix already applied to the LVGL task stack.
    BaseType_t ai_task_result = xTaskCreatePinnedToCoreWithCaps(
        ai_task,
        "object_task",
        16 * 1024,
        NULL,
        3,
        NULL,
        1,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (ai_task_result != pdPASS) {
        ESP_LOGE(TAG, "FAILED to create ai_task (result=%d) — dangerous/non-dangerous object detection will NOT run! Free heap: %u, largest free INTERNAL block: %u",
                 (int)ai_task_result, (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    BaseType_t status_task_result = xTaskCreatePinnedToCoreWithCaps(
        status_task,
        "status_task",
        4096,
        NULL,
        2,
        NULL,
        0,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (status_task_result != pdPASS) {
        ESP_LOGE(TAG, "FAILED to create status_task (result=%d) — no periodic status log/telemetry! Free heap: %u, largest free INTERNAL block: %u",
                 (int)status_task_result, (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }

    ESP_LOGI(
        TAG,
        "AEGIS GUARDIAN ACTIVE"
    );
}