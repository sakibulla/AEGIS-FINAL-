#include "espnow_mesh.h"
#include "swarm_link.h"
#include <esp_log.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>


static const char *TAG = "ESPNOW_MESH_PROD";

static const uint8_t s_pathfinder_slave_mac[6] = {0x00, 0x70, 0x07,
                                                  0x26, 0xB5, 0xB4};
static const uint8_t s_broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static uint16_t s_latest_distances[19];
static SemaphoreHandle_t s_telemetry_sem = NULL;

static void master_recv_cb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int len) {
  if (swarm_link_handle_frame(data, len)) {
    return;
  }

  if (len < sizeof(uint8_t))
    return;

  uint8_t msg_type = data[0];

  if (msg_type == MSG_SCAN_DATA) {
    if (len < sizeof(pathfinder_scan_packet_t))
      return;

    pathfinder_scan_packet_t scan_pkt;
    memcpy(&scan_pkt, data, sizeof(pathfinder_scan_packet_t));

    memcpy(s_latest_distances, scan_pkt.distances_mm,
           sizeof(s_latest_distances));

    if (s_telemetry_sem != NULL) {
      xSemaphoreGive(s_telemetry_sem);
    }
    ESP_LOGI(TAG, "Received 19-point spatial telemetry from Pathfinder Slave.");
  }
}

esp_err_t init_master_espnow(uint8_t wifi_channel) {
  if (s_telemetry_sem == NULL) {
    s_telemetry_sem = xSemaphoreCreateBinary();
  }

  if (esp_now_init() != ESP_OK) {
    ESP_LOGE(TAG, "ESP-NOW Initialization Failed on Master");
    return ESP_FAIL;
  }

  esp_now_register_recv_cb(master_recv_cb);

  // Register Broadcast Peer
  esp_now_peer_info_t bcast_peer = {};
  memcpy(bcast_peer.peer_addr, s_broadcast_mac, 6);
  bcast_peer.channel = wifi_channel;
  bcast_peer.encrypt = false;

  if (!esp_now_is_peer_exist(s_broadcast_mac)) {
    esp_now_add_peer(&bcast_peer);
  }

  // Register the Slave itself. This was missing entirely — send_cmd_to_slave()
  // has always unicast directly to s_pathfinder_slave_mac, but nothing ever
  // called esp_now_add_peer() for it, only for the broadcast address above.
  // Every movement/scan command therefore failed immediately with
  // ESP_ERR_ESPNOW_NOT_FOUND (0x3069) — "peer is not found" — which is why
  // the Slave never moved regardless of Wi-Fi state.
  esp_now_peer_info_t slave_peer = {};
  memcpy(slave_peer.peer_addr, s_pathfinder_slave_mac, 6);
  slave_peer.channel = wifi_channel;
  slave_peer.encrypt = false;

  if (!esp_now_is_peer_exist(s_pathfinder_slave_mac)) {
    esp_err_t add_err = esp_now_add_peer(&slave_peer);
    if (add_err != ESP_OK) {
      ESP_LOGE(TAG, "Failed to add Pathfinder Slave as ESP-NOW peer: %s", esp_err_to_name(add_err));
    }
  } else {
    esp_now_mod_peer(&slave_peer);
  }

  ESP_LOGI(TAG, "ESP-NOW Master active on Wi-Fi Channel %d", wifi_channel);
  return ESP_OK;
}

esp_err_t send_cmd_to_slave(uint8_t cmd, uint16_t duration_or_dist) {
  esp_err_t ret = ESP_FAIL;

  if (cmd == MSG_CMD_MOVE_STEP || cmd == CMD_FWD) {
    cmd_move_step_t pkt = {};
    pkt.msg_type = MSG_CMD_MOVE_STEP;
    pkt.target_id = 0x01;
    pkt.distance_mm = (int16_t)duration_or_dist;
    pkt.turn_deg = 0;
    ret = esp_now_send(s_pathfinder_slave_mac, (const uint8_t *)&pkt,
                       sizeof(pkt));
  } else if (cmd == CMD_REV) {
    cmd_move_step_t pkt = {};
    pkt.msg_type = MSG_CMD_MOVE_STEP;
    pkt.target_id = 0x01;
    pkt.distance_mm = -(int16_t)duration_or_dist; // Negative for reverse
    pkt.turn_deg = 0;
    ret = esp_now_send(s_pathfinder_slave_mac, (const uint8_t *)&pkt,
                       sizeof(pkt));
  } else if (cmd == CMD_LEFT) {
    cmd_move_step_t pkt = {};
    pkt.msg_type = MSG_CMD_MOVE_STEP;
    pkt.target_id = 0x01;
    pkt.distance_mm = 0;
    pkt.turn_deg = -30; // Turn 30° left
    ret = esp_now_send(s_pathfinder_slave_mac, (const uint8_t *)&pkt,
                       sizeof(pkt));
  } else if (cmd == CMD_RIGHT) {
    cmd_move_step_t pkt = {};
    pkt.msg_type = MSG_CMD_MOVE_STEP;
    pkt.target_id = 0x01;
    pkt.distance_mm = 0;
    pkt.turn_deg = 30; // Turn 30° right
    ret = esp_now_send(s_pathfinder_slave_mac, (const uint8_t *)&pkt,
                       sizeof(pkt));
  } else if (cmd == MSG_CMD_SCAN || cmd == CMD_SCAN) {
    cmd_trigger_t pkt = {};
    pkt.msg_type = MSG_CMD_SCAN;
    pkt.target_id = 0x01;
    ret = esp_now_send(s_pathfinder_slave_mac, (const uint8_t *)&pkt,
                       sizeof(pkt));
  } else if (cmd == MSG_CMD_STOP || cmd == CMD_STOP) {
    cmd_trigger_t pkt = {};
    pkt.msg_type = MSG_CMD_STOP;
    pkt.target_id = 0x01;
    ret = esp_now_send(s_pathfinder_slave_mac, (const uint8_t *)&pkt,
                       sizeof(pkt));
  } else {
    cmd_trigger_t pkt = {};
    pkt.msg_type = cmd;
    pkt.target_id = 0x01;
    ret = esp_now_send(s_pathfinder_slave_mac, (const uint8_t *)&pkt,
                       sizeof(pkt));
  }

  if (ret != ESP_OK) {
    ESP_LOGW(TAG,
             "ESP-NOW Command Transmission Failed (Cmd: 0x%02X, Err: 0x%X)",
             cmd, ret);
  }
  return ret;
}

bool wait_for_slave_telemetry(uint16_t out_distances[19], uint32_t timeout_ms) {
  if (s_telemetry_sem == NULL)
    return false;

  if (xSemaphoreTake(s_telemetry_sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
    memcpy(out_distances, s_latest_distances, sizeof(uint16_t) * 19);
    return true;
  }
  return false;
}

esp_err_t broadcast_map_packet(const MapPacket *pkt) {
  if (!pkt)
    return ESP_ERR_INVALID_ARG;
  return esp_now_send(s_broadcast_mac, (const uint8_t *)pkt, sizeof(MapPacket));
}