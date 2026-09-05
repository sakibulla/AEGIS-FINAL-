#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_http_client.h"

#include "protocol_defs.h"
#include "espnow_mesh.h"
#include "vision_task.h"
#include "web_streamer.h"
#include "aegis_swarm_protocol.h"
#include "swarm_link.h"
#include "swarm_display.h"
#include "bsp/esp-bsp.h"

static const char *TAG = "AEGIS_PATHFINDER";

// ============================================================
// AEGIS Backend Server & WiFi Configuration
// ============================================================
#define BOT_ID                     "Pathfinder"
#define BACKEND_SERVER_URL         "http://10.244.86.83:8000"
#define BACKEND_TELEMETRY_ENDPOINT "/api/v1/telemetry/ingest"
#define BACKEND_INCIDENT_ENDPOINT  "/api/v1/incidents/report"
#define BACKEND_SCENARIO_ENDPOINT  "/api/v1/test/current-scenario"
#define BACKEND_HEALTH_ENDPOINT    "/"
#define BACKEND_TIMEOUT_MS         5000

// WiFi Configuration for Backend Connection
#define WIFI_SSID                  "A34"
#define WIFI_PASSWORD              "01234567"
#define WIFI_MAXIMUM_RETRY         10

// Network & Operational State
static int s_wifi_retry_num = 0;
static bool s_wifi_connected = false;
static bool s_backend_online = false;
static char s_local_ip_str[32] = "0.0.0.0";
static int s_wifi_rssi = -60;

static bool s_auto_mode_enabled = true;
static char s_bot_status[16] = "MAPPING";
static int s_simulated_battery_pct = 98;
static int64_t s_battery_timer_ms = 0;

static MapPacket s_baseline_map[10]; // Stores Phase 1 2D grid for Phase 2 drift comparisons

// ── Telemetry Message Queue Types (Guardian-Style Architecture) ───────────
typedef enum {
    TELEMETRY_MSG_STATUS = 0,
    TELEMETRY_MSG_MAP,
    TELEMETRY_MSG_VISION,
    TELEMETRY_MSG_INCIDENT,
    TELEMETRY_MSG_FETCH_INFO
} telemetry_msg_type_t;

typedef struct {
    telemetry_msg_type_t type;

    // Status / Health
    char status[16];
    int battery_pct;

    // SLAM Map Packet
    int total_snaps;
    int snap_index;
    float x_coord;
    float y_coord;
    uint16_t distances_cm[19];
    bool has_door;

    // AI Vision Detection
    char label[32];
    float confidence;
    bool is_threat;
    float bbox_x, bbox_y, bbox_w, bbox_h;

    // Incident Report
    char incident_type[24];
    char incident_message[128];
    char incident_severity[16];
} telemetry_msg_t;

static QueueHandle_t s_telemetry_queue = NULL;

// ── Non-Blocking Telemetry Queue Dispatch Helpers ──────────────────────────
static void queue_status_telemetry(void) {
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_STATUS;
    snprintf(msg.status, sizeof(msg.status), "%s", s_bot_status);
    msg.battery_pct = s_simulated_battery_pct;
    xQueueSend(s_telemetry_queue, &msg, 0);
}

static void queue_map_packet(int total_snaps, int snap_index, float x, float y,
                             const uint16_t *d_mm, bool has_door) {
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_MAP;
    msg.total_snaps = total_snaps;
    msg.snap_index = snap_index;
    msg.x_coord = x;
    msg.y_coord = y;
    msg.has_door = has_door;

    if (d_mm) {
        for (int i = 0; i < 19; i++) {
            msg.distances_cm[i] = d_mm[i] / 10; // Convert mm -> cm
        }
    }
    xQueueSend(s_telemetry_queue, &msg, 0);
}

static void queue_vision_detection(const char *label, float confidence, bool is_threat,
                                  float x, float y, float w, float h) {
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_VISION;
    if (label) snprintf(msg.label, sizeof(msg.label), "%.31s", label);
    msg.confidence = confidence;
    msg.is_threat = is_threat;
    msg.bbox_x = x;
    msg.bbox_y = y;
    msg.bbox_w = w;
    msg.bbox_h = h;
    xQueueSend(s_telemetry_queue, &msg, 0);
}

static void queue_incident_report(const char *type, const char *message, const char *severity) {
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_INCIDENT;
    if (type) snprintf(msg.incident_type, sizeof(msg.incident_type), "%.23s", type);
    if (message) snprintf(msg.incident_message, sizeof(msg.incident_message), "%.127s", message);
    if (severity) snprintf(msg.incident_severity, sizeof(msg.incident_severity), "%.15s", severity);
    xQueueSend(s_telemetry_queue, &msg, 0);
}

static void queue_fetch_backend_info(void) {
    if (!s_telemetry_queue) return;
    telemetry_msg_t msg = {};
    msg.type = TELEMETRY_MSG_FETCH_INFO;
    xQueueSend(s_telemetry_queue, &msg, 0);
}

// ── HTTP Communication Engine with Backend ────────────────────────────────
static esp_err_t backend_http_post(const char *endpoint, const char *json_payload) {
    if (!s_wifi_connected || json_payload == NULL || endpoint == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, endpoint);

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = BACKEND_TIMEOUT_MS;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    config.keep_alive_enable = false;

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
            ESP_LOGD(TAG, "Backend POST %s OK (HTTP %d)", endpoint, status_code);
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

static esp_err_t backend_http_get(const char *endpoint, char *out_buf, size_t max_len) {
    if (!s_wifi_connected || endpoint == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    char url[256];
    snprintf(url, sizeof(url), "%s%s", BACKEND_SERVER_URL, endpoint);

    if (out_buf && max_len > 0) {
        out_buf[0] = '\0';
    }

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = BACKEND_TIMEOUT_MS;
    config.buffer_size = 1024;
    config.buffer_size_tx = 1024;
    config.keep_alive_enable = false;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client for GET %s", endpoint);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        if (status_code >= 200 && status_code < 300) {
            int len = esp_http_client_read_response(client, out_buf, max_len - 1);
            if (len >= 0 && out_buf) {
                out_buf[len] = '\0';
            }
            ESP_LOGD(TAG, "Backend GET %s OK (HTTP %d)", endpoint, status_code);
            s_backend_online = true;
        }
    } else {
        ESP_LOGW(TAG, "Backend GET %s failed: %s", endpoint, esp_err_to_name(err));
        s_backend_online = false;
    }

    esp_http_client_cleanup(client);
    return err;
}

// ── Backend Payload Builders (Safe Bounded JSON Serialization) ─────────────
static void send_status_to_backend(const telemetry_msg_t *msg) {
    char json_str[1024];
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int64_t uptime_sec = esp_timer_get_time() / 1000000;

    snprintf(json_str, sizeof(json_str),
             "{"
             "\"bot_id\":\"Pathfinder\","
             "\"kind\":\"telemetry\","
             "\"payload\":{"
             "\"status\":\"%.15s\","
             "\"battery_pct\":%d,"
             "\"system_info\":{"
             "\"free_heap\":%lu,"
             "\"psram\":%lu"
             "},"
             "\"wifi_rssi\":%d,"
             "\"ip_address\":\"%.19s\","
             "\"timestamp\":\"uptime_%llds\""
             "}"
             "}",
             msg->status[0] ? msg->status : s_bot_status,
             msg->battery_pct,
             (unsigned long)free_heap,
             (unsigned long)free_psram,
             s_wifi_rssi,
             s_local_ip_str,
             (long long)uptime_sec);

    backend_http_post(BACKEND_TELEMETRY_ENDPOINT, json_str);
}

static void send_map_to_backend(const telemetry_msg_t *msg) {
    char distances_json[256] = "[";
    char temp[16];
    for (int i = 0; i < 19; i++) {
        snprintf(temp, sizeof(temp), "%u%s", msg->distances_cm[i], (i < 18) ? "," : "");
        strcat(distances_json, temp);
    }
    strcat(distances_json, "]");

    char json_str[1024];
    snprintf(json_str, sizeof(json_str),
             "{"
             "\"bot_id\":\"Pathfinder\","
             "\"kind\":\"map_packet\","
             "\"payload\":{"
             "\"total_snaps\":%d,"
             "\"snap_index\":%d,"
             "\"x_coord\":%.2f,"
             "\"y_coord\":%.2f,"
             "\"ultrasonic_distances_cm\":%s,"
             "\"has_door\":%s,"
             "\"ip_address\":\"%.19s\""
             "}"
             "}",
             msg->total_snaps, msg->snap_index, msg->x_coord, msg->y_coord,
             distances_json, msg->has_door ? "true" : "false", s_local_ip_str);

    backend_http_post(BACKEND_TELEMETRY_ENDPOINT, json_str);
}

static void send_vision_to_backend(const telemetry_msg_t *msg) {
    char json_str[1024];
    snprintf(json_str, sizeof(json_str),
             "{"
             "\"bot_id\":\"Pathfinder\","
             "\"kind\":\"vision_detection\","
             "\"payload\":{"
             "\"label\":\"%.31s\","
             "\"confidence\":%.1f,"
             "\"is_threat\":%s,"
             "\"bbox\":{"
             "\"x\":%.1f,"
             "\"y\":%.1f,"
             "\"w\":%.1f,"
             "\"h\":%.1f"
             "},"
             "\"ip_address\":\"%.19s\""
             "}"
             "}",
             msg->label, msg->confidence, msg->is_threat ? "true" : "false",
             msg->bbox_x, msg->bbox_y, msg->bbox_w, msg->bbox_h, s_local_ip_str);

    backend_http_post(BACKEND_TELEMETRY_ENDPOINT, json_str);
}

static void send_incident_to_backend(const telemetry_msg_t *msg) {
    char json_str[1024];
    snprintf(json_str, sizeof(json_str),
             "{"
             "\"bot_id\":\"Pathfinder\","
             "\"type\":\"%.31s\","
             "\"severity\":\"%.15s\","
             "\"message\":\"%.127s\""
             "}",
             msg->incident_type[0] ? msg->incident_type : "INTRUDER",
             msg->incident_severity[0] ? msg->incident_severity : "CRITICAL",
             msg->incident_message[0] ? msg->incident_message : "Security alert from Pathfinder");

    backend_http_post(BACKEND_INCIDENT_ENDPOINT, json_str);
}

static void fetch_backend_info(void) {
    char response[256] = {0};
    esp_err_t err = backend_http_get(BACKEND_HEALTH_ENDPOINT, response, sizeof(response));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Backend Health Check: %s", response[0] ? response : "ONLINE");
    }

    char scenario_resp[512] = {0};
    err = backend_http_get(BACKEND_SCENARIO_ENDPOINT, scenario_resp, sizeof(scenario_resp));
    if (err == ESP_OK && scenario_resp[0] != '\0') {
        ESP_LOGI(TAG, "Active Backend Swarm Scenario: %s", scenario_resp);
    }
}

// ── Background Telemetry Task (Consumer Loop) ─────────────────────────────
static void telemetry_task(void *arg) {
    ESP_LOGI(TAG, "Telemetry dispatcher task active CPU%d", xPortGetCoreID());
    telemetry_msg_t msg;

    while (true) {
        if (xQueueReceive(s_telemetry_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.type) {
                case TELEMETRY_MSG_STATUS:
                    send_status_to_backend(&msg);
                    break;
                case TELEMETRY_MSG_MAP:
                    send_map_to_backend(&msg);
                    break;
                case TELEMETRY_MSG_VISION:
                    send_vision_to_backend(&msg);
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

// ── Periodic Heartbeat & Battery Simulation Task ──────────────────────────
static void backend_heartbeat_task(void *pvParameters) {
    ESP_LOGI(TAG, "Heartbeat sync task started CPU%d", xPortGetCoreID());
    s_battery_timer_ms = esp_timer_get_time() / 1000;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(3000));

        if (s_wifi_connected) {
            // Simulated battery discharge (1% every 45 seconds)
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (now_ms - s_battery_timer_ms > 45000) {
                if (s_simulated_battery_pct > 25) {
                    s_simulated_battery_pct--;
                }
                s_battery_timer_ms = now_ms;
            }

            queue_status_telemetry();
        }
    }
}

// ── AI Vision Stream Task (Edge Impulse Detections) ───────────────────────
static void pathfinder_vision_stream_task(void *pvParameters) {
    ESP_LOGI(TAG, "Vision streaming task active CPU%d", xPortGetCoreID());
    detection_t dets[MAX_DETECTIONS];
    int count = 0;
    int latency_ms = 0;

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(500));

        if (!s_wifi_connected) continue;

        get_current_detections(dets, &count, &latency_ms);

        for (int i = 0; i < count; i++) {
            bool is_threat = (strstr(dets[i].label, "threat") != NULL ||
                              strstr(dets[i].label, "intruder") != NULL ||
                              strstr(dets[i].label, "knife") != NULL ||
                              strstr(dets[i].label, "gun") != NULL);

            queue_vision_detection(dets[i].label, dets[i].score * 100.0f, is_threat,
                                   (float)dets[i].x, (float)dets[i].y,
                                   (float)dets[i].w, (float)dets[i].h);

            if (is_threat) {
                char alert_msg[128];
                snprintf(alert_msg, sizeof(alert_msg),
                         "Security Threat: %.31s detected with %.1f%% confidence",
                         dets[i].label, dets[i].score * 100.0f);
                queue_incident_report("INTRUDER", alert_msg, "CRITICAL");
                snprintf(s_bot_status, sizeof(s_bot_status), "ALERT");
            }
        }
    }
}

// ── Wi-Fi Event Handler ───────────────────────────────────────────────────
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // Deliberately NOT connecting here — see the identical fix (and its
        // full explanation) in Guardian's main.cpp. STA_START fires the
        // instant esp_wifi_start() returns inside init_wifi_and_espnow(),
        // well before swarm_link_init() runs later in app_main() (camera,
        // display, and ESP-NOW master init sit in between). Connecting this
        // early meant the STA was already mid-connection by the time
        // swarm_link_init() tried to pin channel 6, so the pin silently
        // failed ("STA is scanning or connecting... cannot set channel").
        // The first connection attempt is now kicked off explicitly from
        // app_main() after swarm_link_init() has already pinned the channel.
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
            ESP_LOGW(TAG, "WiFi retry limit reached, reconnecting shortly");
            s_wifi_retry_num = 0;
            vTaskDelay(pdMS_TO_TICKS(5000));
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        snprintf(s_local_ip_str, sizeof(s_local_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Connected to WiFi! IP: %s", s_local_ip_str);
        s_wifi_connected = true;
        s_wifi_retry_num = 0;

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_wifi_rssi = ap_info.rssi;
            ESP_LOGI(TAG, "Wi-Fi AP Connected on Channel: %d (RSSI: %d dBm)", ap_info.primary, ap_info.rssi);
            init_master_espnow(ap_info.primary);
        } else {
            init_master_espnow(6);
        }

        // Immediately ping backend and transmit initial telemetry
        queue_fetch_backend_info();
        queue_status_telemetry();
    }
}

// ── Swarm: hazard evacuation (Warden fire/smoke/gas -> head for an exit) ──
// Pathfinder has no odometry or position tracking, so this isn't a true
// "navigate to (x,y)" — it turns around and re-scans as it drives, stopping
// at the first opening that matches the exact same door criterion used
// during mapping (>1800mm clear ahead). That's simpler and more honest than
// dead-reckoning a step count back to a specific previously-recorded snap,
// and it's genuinely achievable with the movement/sensing primitives this
// bot already has.
static volatile bool g_evacuate_to_exit = false;

// ── Duty cycle: run 5 minutes, rest 5 minutes, repeat ──────────────────────
// Pathfinder now scouts on its own from the moment it boots: no dependency
// on Guardian's WAKE or on backend Wi-Fi being up. (Previously this was
// gated on a WAKE-triggered 5-minute one-shot timer per SWARM_ESPNOW_DESIGN.md
// §8a; that meant a standalone Pathfinder — no Guardian in range — never
// moved at all, which is what prompted this change.)
#define DUTY_CYCLE_RUN_MS   (5ULL * 60ULL * 1000ULL)
#define DUTY_CYCLE_REST_MS  (5ULL * 60ULL * 1000ULL)

static inline int64_t now_ms() {
    return esp_timer_get_time() / 1000;
}

// Blocks for the rest window, but stays responsive to hazard evacuation so a
// resting bot still moves the moment Warden raises an alert.
static void duty_cycle_rest() {
    ESP_LOGI(TAG, "Duty cycle: run window elapsed -> resting for 5 minutes");
    snprintf(s_bot_status, sizeof(s_bot_status), "RESTING");
    queue_status_telemetry();

    int64_t rest_until_ms = now_ms() + DUTY_CYCLE_REST_MS;
    while (now_ms() < rest_until_ms) {
        if (g_evacuate_to_exit || !s_auto_mode_enabled) {
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    ESP_LOGI(TAG, "Duty cycle: rest complete -> resuming scouting");
}

static void on_swarm_hazard_alert(uint8_t hazard_bits) {
    (void)hazard_bits;
    if (!g_evacuate_to_exit) {
        ESP_LOGW(TAG, "Swarm: Warden hazard alert -> Pathfinder heading for nearest exit");
        g_evacuate_to_exit = true;
    }
}

static void on_swarm_all_clear(void) {
    if (g_evacuate_to_exit) {
        ESP_LOGI(TAG, "Swarm: ALL_CLEAR received -> ending evacuation");
        g_evacuate_to_exit = false;
    }
}

// Turns around and drives forward, re-scanning each step, until it finds an
// opening (same >1800mm criterion as door detection during mapping) or hits
// a step cap — then holds position until g_evacuate_to_exit clears.
static void evacuate_to_exit_door() {
    ESP_LOGW(TAG, "=== EVACUATION: heading for nearest known exit ===");
    snprintf(s_bot_status, sizeof(s_bot_status), "EVACUATING");
    queue_status_telemetry();

    ESP_LOGI(TAG, "Evacuation: turning around...");
    send_cmd_to_slave(CMD_RIGHT, 800);
    vTaskDelay(pdMS_TO_TICKS(1200));

    const int MAX_EVAC_STEPS = 12;
    for (int step = 0; step < MAX_EVAC_STEPS && g_evacuate_to_exit; step++) {
        uint16_t distances[19] = {0};
        send_cmd_to_slave(CMD_SCAN, 0);

        if (wait_for_slave_telemetry(distances, 6000) && distances[9] > 1800) {
            ESP_LOGW(TAG, "Evacuation: opening found (%umm ahead) — treating as exit, holding position", distances[9]);
            break;
        }

        ESP_LOGI(TAG, "Evacuation: no opening yet, advancing (step %d/%d)...", step + 1, MAX_EVAC_STEPS);
        send_cmd_to_slave(CMD_FWD, 400);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGW(TAG, "=== EVACUATION: holding at exit, waiting for ALL_CLEAR ===");
    snprintf(s_bot_status, sizeof(s_bot_status), "AT_EXIT");
    queue_status_telemetry();

    while (g_evacuate_to_exit) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "=== EVACUATION: hazard cleared, resuming normal scouting ===");
}

// ── Autonomous Mapping & SLAM Task ────────────────────────────────────────
static void pathfinder_mapping_task(void *pvParameters) {
    ESP_LOGI(TAG, "Pathfinder autonomous SLAM task active CPU%d", xPortGetCoreID());

    // NOTE: this used to block here on `while (!s_wifi_connected)` before
    // doing anything at all. That's backend Wi-Fi specifically, not swarm
    // ESP-NOW — which is already fully initialized by this point regardless
    // (init_master_espnow()/swarm_link_init() both run before
    // esp_wifi_connect() is even called in init_wifi_and_espnow()). Blocking
    // scouting on backend Wi-Fi directly contradicted the swarm design's own
    // "must keep working even if the backend AP is down" goal — a bot with
    // no backend Wi-Fi would never even reach the WAKE-wait gate below, let
    // alone start moving. Every telemetry/HTTP call inside this task already
    // self-guards on s_wifi_connected internally and no-ops safely when
    // disconnected, so there's nothing this wait was actually protecting.
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (true) {
        if (g_evacuate_to_exit) {
            evacuate_to_exit_door();
            continue;
        }

        if (!s_auto_mode_enabled) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        // Duty cycle: this run window lasts 5 minutes from now, across both
        // Phase 1 and Phase 2 below, then a 5-minute rest before looping back.
        int64_t run_until_ms = now_ms() + DUTY_CYCLE_RUN_MS;

        snprintf(s_bot_status, sizeof(s_bot_status), "MAPPING");
        queue_status_telemetry();

        ESP_LOGI(TAG, "=== STARTING PHASE 1: INITIAL 2D MAPPING RUN (10 SNAPS) ===");

        for (uint8_t snap = 0; snap < 10; snap++) {
            if (!s_auto_mode_enabled || g_evacuate_to_exit || now_ms() >= run_until_ms) break;

            ESP_LOGI(TAG, "--> Phase 1: Requesting Map Snapshot [%d/10] from Slave...", snap + 1);

            int retry_count = 0;
            const int MAX_RETRIES = 5;
            bool sweep_success = false;
            uint16_t distances[19] = {0};

            while (!sweep_success && retry_count < MAX_RETRIES) {
                if (!s_auto_mode_enabled) break;

                send_cmd_to_slave(CMD_SCAN, 0);

                if (wait_for_slave_telemetry(distances, 6000)) {
                    sweep_success = true;
                } else {
                    retry_count++;
                    ESP_LOGW(TAG, "Slave sweep timeout. Retrying (%d/%d)...", retry_count, MAX_RETRIES);
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
            }

            if (sweep_success) {
                bool door_found = false;
                for (int i = 0; i < 19; i++) {
                    if (distances[i] > 1800) {
                        door_found = true;
                        break;
                    }
                }

                // Normalized tactical floorplan coordinates for dashboard map (0.18 -> 0.72)
                float norm_x = 0.18f + ((float)snap * 0.058f);
                float norm_y = 0.50f;

                // Cache baseline for Phase 2 drift comparisons
                s_baseline_map[snap].totalSnaps = 10;
                s_baseline_map[snap].snapIndex = snap;
                s_baseline_map[snap].x = norm_x;
                s_baseline_map[snap].y = norm_y;
                s_baseline_map[snap].has_door = door_found;
                memcpy(s_baseline_map[snap].d, distances, sizeof(distances));

                // Transmit SLAM map packet to backend
                queue_map_packet(10, snap, norm_x, norm_y, distances, door_found);

                // Broadcast over ESP-NOW mesh
                broadcast_map_packet(&s_baseline_map[snap]);
                swarm_send_map_update(norm_x, norm_y, door_found, snap, 10);

                if (door_found) {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Doorway aperture identified at floorplan coordinate (%.2f, %.2f)", norm_x, norm_y);
                    queue_incident_report("DOOR_DETECTED", msg, "INFO");
                }
            }

            // Step Forward (except on last snap)
            if (snap < 9 && s_auto_mode_enabled) {
                ESP_LOGI(TAG, "--> Stepping forward to next mapping sector...");
                send_cmd_to_slave(CMD_FWD, 400);
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

        ESP_LOGI(TAG, "=== PHASE 1 COMPLETE: BASELINE 2D GRID STORED ===");

        if (g_evacuate_to_exit) {
            continue; // outer loop's top handles evacuation
        }

        if (now_ms() >= run_until_ms) {
            duty_cycle_rest();
            continue;
        }

        // Turn 180 degrees to begin Phase 2 Patrol
        if (s_auto_mode_enabled) {
            ESP_LOGI(TAG, "Executing 180-degree turnaround for Phase 2 Patrol...");
            send_cmd_to_slave(CMD_RIGHT, 800);
            vTaskDelay(pdMS_TO_TICKS(1200));
        }

        // ── Phase 2: Autonomous Patrol & Room Drift Verification ────────────
        snprintf(s_bot_status, sizeof(s_bot_status), "PATROL");
        queue_status_telemetry();

        int patrol_cycle = 0;
        while (s_auto_mode_enabled && !g_evacuate_to_exit && now_ms() < run_until_ms) {
            patrol_cycle++;
            ESP_LOGI(TAG, "=== PATROL CYCLE %d: CONTINUOUS DRIFT MONITORING ===", patrol_cycle);

            for (uint8_t snap = 0; snap < 10; snap++) {
                if (!s_auto_mode_enabled || g_evacuate_to_exit || now_ms() >= run_until_ms) break;

                uint16_t current_dist[19] = {0};
                send_cmd_to_slave(CMD_SCAN, 0);

                if (wait_for_slave_telemetry(current_dist, 6000)) {
                    // Drift check against Phase 1 baseline
                    int drift_count = 0;
                    for (int i = 0; i < 19; i++) {
                        if (s_baseline_map[snap].d[i] > 0) {
                            int diff = abs((int)current_dist[i] - (int)s_baseline_map[snap].d[i]);
                            if (diff > 200) { // 200mm environmental shift
                                drift_count++;
                            }
                        }
                    }

                    float norm_x = s_baseline_map[snap].x;
                    float norm_y = s_baseline_map[snap].y;
                    bool door_found = (current_dist[9] > 1800);

                    // Transmit live map update
                    queue_map_packet(10, snap, norm_x, norm_y, current_dist, door_found);

                    if (drift_count >= 3) {
                        char alert_msg[128];
                        snprintf(alert_msg, sizeof(alert_msg),
                                 "Environmental shift detected at coordinate (%.2f, %.2f) across %d angles",
                                 norm_x, norm_y, drift_count);
                        queue_incident_report("ENVIRONMENTAL_SHIFT", alert_msg, "HIGH");
                        snprintf(s_bot_status, sizeof(s_bot_status), "ALERT");
                    }
                }

                if (snap < 9 && s_auto_mode_enabled) {
                    send_cmd_to_slave(CMD_FWD, 400);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }

            // Turnaround for next patrol loop
            if (s_auto_mode_enabled && !g_evacuate_to_exit && now_ms() < run_until_ms) {
                send_cmd_to_slave(CMD_RIGHT, 800);
                vTaskDelay(pdMS_TO_TICKS(1200));
            }
        }

        if (!g_evacuate_to_exit && now_ms() >= run_until_ms) {
            duty_cycle_rest();
        }
    }
}

// ── External Mode Toggle (Called from Web Streamer) ───────────────────────
void set_auto_mode(bool enable) {
    s_auto_mode_enabled = enable;
    if (enable) {
        snprintf(s_bot_status, sizeof(s_bot_status), "MAPPING");
        ESP_LOGI(TAG, "Mode toggled: AUTO SLAM ACTIVE");
    } else {
        snprintf(s_bot_status, sizeof(s_bot_status), "PATROL");
        ESP_LOGI(TAG, "Mode toggled: MANUAL RC CONTROL ACTIVE");
    }
    queue_status_telemetry();
}

// ── Wi-Fi & ESP-NOW Dual Initialization ───────────────────────────────────
static bool init_wifi_and_espnow(void) {
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

    // Print Pathfinder MAC Address for Swarm Discovery
    uint8_t local_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, local_mac);
    ESP_LOGI(TAG, "Pathfinder MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             local_mac[0], local_mac[1], local_mac[2], 
             local_mac[3], local_mac[4], local_mac[5]);

    // Initialize ESP-NOW with the Slave's actual fixed channel. This was
    // hardcoded to 9 (stale — the Slave (espnow_slave.h: WIFI_CHANNEL) has
    // only ever listened on channel 6, matching the swarm channel), so the
    // ESP-NOW peer entry never matched the Slave's real channel here either.
    init_master_espnow(AEGIS_SWARM_CHANNEL);

    // Swarm on-device status display (SWARM_ESPNOW_DESIGN.md §11) — bring up
    // the integrated LCD/LVGL stack before the swarm link, so swarm_link can
    // push updates to it as soon as it starts hearing from siblings.
    // Explicit config: the default bsp_display_start() requests a full-frame
    // (240x240) LVGL draw buffer from internal SRAM, which is already crowded
    // by Wi-Fi/camera/AI buffers and fails ("Not enough memory for LVGL
    // buffer") on this board. We only ever draw a 240x40 strip, so request a
    // buffer sized for that instead — small enough (~19KB) to fit comfortably
    // back in internal SRAM with the BSP's normal DMA path (avoids the visual
    // tearing/artifacts a full-frame PSRAM-sourced DMA buffer produced here).
    bsp_display_cfg_t disp_cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * 40,
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

    // Swarm ESP-NOW link (Master-to-Master, independent of the Master<->Slave
    // link above) — see SWARM_ESPNOW_DESIGN.md. Init before esp_wifi_connect()
    // below so the channel-6 pin (§2) applies while STA is still unassociated.
    if (swarm_link_init() == ESP_OK) {
        swarm_link_set_all_clear_cb(on_swarm_all_clear);
        swarm_link_set_hazard_alert_cb(on_swarm_hazard_alert);
    }

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s ...", WIFI_SSID);
    esp_wifi_connect();

    return true;
}

// ── Application Entrypoint (app_main) ─────────────────────────────────────
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "   A.E.G.I.S. PATHFINDER MASTER NODE (ESP32-S3)   ");
    ESP_LOGI(TAG, "   Dual-Core SLAM Mapping & Vision Intelligence   ");
    ESP_LOGI(TAG, "==================================================");

    // Power management: Lock S3 to maximum 240MHz performance
    esp_pm_config_t pm_config = {
        .max_freq_mhz = 240,
        .min_freq_mhz = 240,
        .light_sleep_enable = false
    };
    esp_pm_configure(&pm_config);

    // Initialize Non-Volatile Storage (NVS)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Create telemetry message queue (20 slots)
    s_telemetry_queue = xQueueCreate(20, sizeof(telemetry_msg_t));
    if (!s_telemetry_queue) {
        ESP_LOGE(TAG, "Telemetry queue creation failed!");
    }

    // Start background telemetry dispatcher task (Consumer on Core 0)
    xTaskCreatePinnedToCore(
        telemetry_task,
        "telemetry_task",
        8192,
        NULL,
        2,
        NULL,
        0
    );

    // Initialize Wi-Fi & ESP-NOW Dual Protocol Stacks
    init_wifi_and_espnow();

    // Start Edge Impulse AI Vision Inference Engine & Hardware Camera on Core 1
    start_vision_task();

    // Start HTTP Web Streaming Server on Core 0 (/stream, /snapshot, /detections, /control)
    start_web_streamer();

    // Start Pathfinder Autonomous SLAM State Machine on Core 1
    xTaskCreatePinnedToCore(
        pathfinder_mapping_task,
        "mapping_task",
        8192,
        NULL,
        3,
        NULL,
        1
    );

    // Start AI Vision Detection Sync Task on Core 0
    xTaskCreatePinnedToCore(
        pathfinder_vision_stream_task,
        "vision_sync_task",
        4096,
        NULL,
        2,
        NULL,
        0
    );

    // Start Periodic Telemetry & Heartbeat Sync Task on Core 0
    xTaskCreatePinnedToCore(
        backend_heartbeat_task,
        "heartbeat_task",
        4096,
        NULL,
        2,
        NULL,
        0
    );

    ESP_LOGI(TAG, "AEGIS Pathfinder Master initialization complete!");
}