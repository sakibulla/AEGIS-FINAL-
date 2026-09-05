#include "espnow_slave.h"
#include "motor_control.h"
#include "servo_sweep.h"
#include "ultrasonic_sensor.h"
#include <string.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

static const char *TAG = "ESPNOW_SLAVE";
static uint8_t s_master_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef enum {
    CMD_TYPE_DRIVE,
    CMD_TYPE_MOVE_STEP,
    CMD_TYPE_SCAN,
    CMD_TYPE_STOP
} slave_cmd_type_t;

typedef struct {
    slave_cmd_type_t type;
    int8_t  linear_vel;
    int8_t  angular_vel;
    int16_t distance_mm;
    int16_t turn_deg;
} slave_cmd_t;

static QueueHandle_t s_cmd_queue = NULL;

static void ensure_master_peer_registered(const uint8_t *mac) {
    if (memcmp(mac, "\x00\x00\x00\x00\x00\x00", 6) == 0 || 
        memcmp(mac, "\xFF\xFF\xFF\xFF\xFF\xFF", 6) == 0) return;

    if (!esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, mac, 6);
        peerInfo.channel = WIFI_CHANNEL;
        peerInfo.encrypt = false;
        
        esp_err_t add_status = esp_now_add_peer(&peerInfo);
        if (add_status == ESP_OK) {
            ESP_LOGI(TAG, "Registered Master Peer: %02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        } else {
            ESP_LOGE(TAG, "Failed to register Master peer: 0x%x", add_status);
        }
    }
}

// 19-Point Spatial Mapping Sweep returned to Master
static void execute_spatial_mapping_sweep(void) {
    pathfinder_scan_packet_t packet;
    packet.msg_type     = MSG_SCAN_DATA;
    packet.sender_id    = PATHFINDER_NODE_ID;
    packet.pos_x        = 0.0f;
    packet.pos_y        = 0.0f;
    packet.status_flags = 0x01;

    ESP_LOGI(TAG, "Executing 19-point Mapping Sweep for Master...");

    for (int i = 0; i < 19; i++) {
        uint8_t angle = i * 10;
        set_servo_angle(angle);
        vTaskDelay(pdMS_TO_TICKS(40));
        
        uint16_t dist = measure_distance_mm();
        packet.distances_mm[i] = dist;
    }

    set_servo_angle(90); // Center servo

    ensure_master_peer_registered(s_master_mac);

    esp_err_t result = esp_now_send(s_master_mac, (uint8_t *)&packet, sizeof(packet));
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "Mapping sweep complete. Broadcasted packet to Master.");
    } else {
        ESP_LOGE(TAG, "Failed to transmit scan packet over ESP-NOW (0x%x)", result);
    }
}

static void slave_worker_task(void *pvParameters) {
    slave_cmd_t cmd;
    while (1) {
        if (xQueueReceive(s_cmd_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.type) {
                case CMD_TYPE_DRIVE:
                    drive_motors(cmd.linear_vel, cmd.angular_vel);
                    break;

                case CMD_TYPE_MOVE_STEP:
                    execute_step_move(cmd.distance_mm, cmd.turn_deg);
                    break;

                case CMD_TYPE_SCAN:
                    execute_spatial_mapping_sweep();
                    break;

                case CMD_TYPE_STOP:
                    emergency_stop_motors();
                    break;
            }
        }
    }
}

static void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
    if (len < sizeof(uint8_t)) return;

    memcpy(s_master_mac, recv_info->src_addr, 6);
    ensure_master_peer_registered(s_master_mac);

    uint8_t msg_type = incomingData[0];
    slave_cmd_t cmd = {};

    if (msg_type == MSG_CMD_DRIVE) {
        if (len < sizeof(cmd_drive_t)) return;
        cmd_drive_t drive_pkt;
        memcpy(&drive_pkt, incomingData, sizeof(cmd_drive_t));

        if (drive_pkt.target_id == PATHFINDER_NODE_ID || drive_pkt.target_id == 0xFF) {
            cmd.type = CMD_TYPE_DRIVE;
            cmd.linear_vel = drive_pkt.linear_vel;
            cmd.angular_vel = drive_pkt.angular_vel;
            xQueueSendFromISR(s_cmd_queue, &cmd, NULL);
        }
    } 
    else if (msg_type == MSG_CMD_SCAN) {
        cmd.type = CMD_TYPE_SCAN;
        xQueueSendFromISR(s_cmd_queue, &cmd, NULL);
    }
    else if (msg_type == MSG_CMD_MOVE_STEP) {
        if (len < sizeof(cmd_move_step_t)) return;
        cmd_move_step_t step_pkt;
        memcpy(&step_pkt, incomingData, sizeof(cmd_move_step_t));

        cmd.type = CMD_TYPE_MOVE_STEP;
        cmd.distance_mm = step_pkt.distance_mm;
        cmd.turn_deg = step_pkt.turn_deg;
        xQueueSendFromISR(s_cmd_queue, &cmd, NULL);
    }
    else if (msg_type == MSG_CMD_STOP) {
        cmd.type = CMD_TYPE_STOP;
        xQueueSendFromISR(s_cmd_queue, &cmd, NULL);
    }
}

esp_err_t init_slave_espnow(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    s_cmd_queue = xQueueCreate(10, sizeof(slave_cmd_t));

    esp_netif_init();
    esp_event_loop_create_default();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (esp_now_init() != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW Initialization Failed");
        return ESP_FAIL;
    }

    esp_now_register_recv_cb(OnDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memset(peerInfo.peer_addr, 0xFF, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    xTaskCreatePinnedToCore(slave_worker_task, "slave_worker", 4096, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "ESP-NOW Pathfinder Slave active on Wi-Fi Channel %d", WIFI_CHANNEL);
    return ESP_OK;
}