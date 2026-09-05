#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "espnow_master.h"
#include "GuardianProtocol.h"
#include "swarm_link.h"

static const char *TAG = "ESPNOW_MASTER";

static const uint8_t SLAVE_MAC[ESP_NOW_ETH_ALEN] = {
    0x00, 0x70, 0x07, 0x7E, 0x65, 0xE0
};

static bool s_espnow_ready = false;
static uint8_t s_current_channel = 1;

/* ============================================================
 * Send Callback
 * ============================================================ */
static void send_callback(const wifi_tx_info_t *tx_info, esp_now_send_status_t status)
{
    if (tx_info == NULL) return;

    // In ESP-IDF 5.5+, we can't access MAC directly from tx_info
    // Just log the status without MAC address
    ESP_LOGI(TAG, "Send Status: %s",
             (status == ESP_NOW_SEND_SUCCESS) ? "SUCCESS" : "FAIL");
}

/* ============================================================
 * Receive Callback
 * ============================================================ */
static void receive_callback(const esp_now_recv_info_t *rx_info,
                             const uint8_t *data, int len)
{
    if (rx_info == NULL || data == NULL || len <= 0) return;

    if (swarm_link_handle_frame(data, len)) {
        return;
    }

    ESP_LOGI(TAG, "Received %d bytes", len);
}

/* ============================================================
 * Initialize ESP-NOW Master
 * ============================================================ */
esp_err_t espnow_master_init(void)
{
    ESP_LOGI(TAG, "=== Initializing ESP-NOW Master ===");

    esp_err_t ret;

    // Make sure Wi-Fi is running
    ret = esp_wifi_start();
    if (ret != ESP_OK && ret != ESP_ERR_WIFI_STATE) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "set_mode failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // ========== GET THE REAL CHANNEL ==========
    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    ret = esp_wifi_get_channel(&primary, &second);

    if (ret != ESP_OK || primary == 0) {
        ESP_LOGW(TAG, "Could not get channel, using 1");
        primary = 1;
    }

    s_current_channel = primary;
    ESP_LOGI(TAG, "Current Wi-Fi channel (home channel) = %d", s_current_channel);
    // ==========================================

    // Print Guardian MAC Address for Swarm Discovery
    uint8_t local_mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, local_mac);
    ESP_LOGI(TAG, "Guardian MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             local_mac[0], local_mac[1], local_mac[2], 
             local_mac[3], local_mac[4], local_mac[5]);

    // Initialize ESP-NOW
    ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "esp_now_init OK");

    esp_now_register_send_cb(send_callback);
    esp_now_register_recv_cb(receive_callback);

    // Add peer using channel 0 (uses active home Wi-Fi channel)
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, SLAVE_MAC, 6);
    peer.channel = 0;     // 0 = use home Wi-Fi channel (prevents ESP_ERR_ESPNOW_CHAN)
    peer.encrypt = false;
    peer.ifidx   = WIFI_IF_STA;

    if (!esp_now_is_peer_exist(SLAVE_MAC)) {
        ret = esp_now_add_peer(&peer);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "add_peer failed: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "Peer added on home channel");
    }

    s_espnow_ready = true;
    ESP_LOGI(TAG, "=== ESP-NOW Master READY ===");
    return ESP_OK;
}

/* ============================================================
 * Sync Channel with Wi-Fi AP
 * ============================================================ */
esp_err_t espnow_master_sync_channel(uint8_t channel)
{
    if (!s_espnow_ready) return ESP_FAIL;
    s_current_channel = channel;

    esp_now_peer_info_t peer = {};
    if (esp_now_get_peer(SLAVE_MAC, &peer) == ESP_OK) {
        peer.channel = channel;
        esp_err_t err = esp_now_mod_peer(&peer);
        ESP_LOGI(TAG, "ESP-NOW Peer updated to channel %d: %s", channel, esp_err_to_name(err));
        return err;
    }
    return ESP_FAIL;
}

/* ============================================================
 * Send command
 * ============================================================ */
esp_err_t espnow_master_send_command(guardian_command_t command)
{
    if (!s_espnow_ready) {
        ESP_LOGE(TAG, "ESP-NOW not ready!");
        return ESP_ERR_ESPNOW_NOT_INIT;
    }

    ESP_LOGI(TAG, "Sending command: %d", (int)command);

    esp_err_t ret = esp_now_send(SLAVE_MAC, (uint8_t *)&command, sizeof(command));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(ret));
    }
    return ret;
}