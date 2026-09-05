#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_timer.h"

#include "aegis_swarm_protocol.h"
#include "swarm_link.h"
#include "swarm_display.h"

static const char *TAG = "SWARM_LINK";

static const swarm_node_id_t SELF_NODE_ID = SWARM_NODE_GUARDIAN;

static const uint8_t s_broadcast_mac[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint32_t s_send_seq = 0;
static esp_timer_handle_t s_heartbeat_timer = nullptr;
static swarm_hazard_alert_cb_t s_hazard_alert_cb = nullptr;
static swarm_all_clear_cb_t s_all_clear_cb = nullptr;

/* s_hazard_alert_cb/s_all_clear_cb are NOT invoked directly from
 * swarm_link_handle_frame() — that runs in the ESP-NOW recv callback, i.e.
 * the Wi-Fi driver task's own context, and on_swarm_hazard_alert() in
 * main.cpp calls espnow_master_send_command() (an ESP-NOW send + a serial
 * log), which is exactly the kind of blocking work that context can't
 * afford (same reasoning as the "don't touch the display here" rule below —
 * it stalled the wake-word audio pipeline's fetch task in practice). Instead
 * just record the pending state here; heartbeat_timer_cb() below is the only
 * place that actually invokes these callbacks. */
static volatile uint8_t s_pending_hazard_bits = 0;
static volatile bool s_hazard_changed = false;

/* last_seen_ms / online / alert_text, indexed by swarm_node_id_t (1..3); index 0 unused */
static int64_t s_last_seen_ms[4] = {0, 0, 0, 0};
static bool s_online[4] = {false, false, false, false};
static char s_alert_text[4][24] = {{0}, {0}, {0}, {0}};

static int64_t now_ms()
{
    return esp_timer_get_time() / 1000;
}

/* Adds a peer at the given channel, or retunes it to that channel if it
 * already exists under a different one (e.g. Pathfinder's pre-existing
 * broadcast peer, registered at its Master<->Slave channel) — avoids
 * ESP_ERR_ESPNOW_EXIST from a blind esp_now_add_peer(). */
static esp_err_t ensure_peer(const uint8_t mac[6], uint8_t channel)
{
    if (esp_now_is_peer_exist(mac)) {
        esp_now_peer_info_t existing = {};
        if (esp_now_get_peer(mac, &existing) == ESP_OK && existing.channel != channel) {
            existing.channel = channel;
            return esp_now_mod_peer(&existing);
        }
        return ESP_OK;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, 6);
    peer.channel = channel;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;
    return esp_now_add_peer(&peer);
}

void swarm_link_set_hazard_alert_cb(swarm_hazard_alert_cb_t cb)
{
    s_hazard_alert_cb = cb;
}

void swarm_link_set_all_clear_cb(swarm_all_clear_cb_t cb)
{
    s_all_clear_cb = cb;
}

void swarm_link_pin_channel(void)
{
    esp_err_t err = esp_wifi_set_channel(AEGIS_SWARM_CHANNEL, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        /* Expected while STA is associated to an AP (channel is locked in that
         * state) — see SWARM_ESPNOW_DESIGN.md §2. Only a problem if the AP is
         * not actually pinned to channel 6. */
        ESP_LOGD(TAG, "pin_channel(%d): %s (expected while STA-associated)",
                 AEGIS_SWARM_CHANNEL, esp_err_to_name(err));
    }
}

static esp_err_t send_packet(swarm_msg_type_t type, const void *payload, size_t payload_len)
{
    swarm_packet_t pkt = {};
    pkt.magic = SWARM_MAGIC;
    pkt.type = (uint8_t)type;
    pkt.source = (uint8_t)SELF_NODE_ID;
    pkt.sequence = ++s_send_seq;
    pkt.uptime_ms = (uint32_t)now_ms();

    if (payload != nullptr && payload_len > 0) {
        memcpy(&pkt.payload, payload, payload_len);
    }

    esp_err_t err = esp_now_send(s_broadcast_mac, (const uint8_t *)&pkt, sizeof(pkt));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "send type=0x%02X failed: %s", (int)type, esp_err_to_name(err));
    }
    return err;
}

esp_err_t swarm_send_wake(void)
{
    ESP_LOGI(TAG, "Swarm: broadcasting WAKE");
    return send_packet(SWARM_MSG_WAKE, nullptr, 0);
}

esp_err_t swarm_send_all_clear(void)
{
    ESP_LOGI(TAG, "Swarm: broadcasting ALL_CLEAR");
    return send_packet(SWARM_MSG_ALL_CLEAR, nullptr, 0);
}

esp_err_t swarm_send_security_alert(bool is_intruder, bool is_dangerous_object,
                                     float confidence, const char *label)
{
    struct {
        uint8_t  is_intruder;
        uint8_t  is_dangerous_object;
        float    confidence;
        char     label[24];
    } payload = {};

    payload.is_intruder = is_intruder ? 1 : 0;
    payload.is_dangerous_object = is_dangerous_object ? 1 : 0;
    payload.confidence = confidence;
    if (label != nullptr) {
        snprintf(payload.label, sizeof(payload.label), "%s", label);
    }

    ESP_LOGW(TAG, "Swarm: broadcasting SECURITY_ALERT (%s, %.1f%%)",
             payload.label, confidence * 100.0f);
    return send_packet(SWARM_MSG_SECURITY_ALERT, &payload, sizeof(payload));
}

static void mark_seen(uint8_t source)
{
    if (source < 1 || source > 3) {
        return;
    }
    s_last_seen_ms[source] = now_ms();
}

bool swarm_link_handle_frame(const uint8_t *data, int len)
{
    if (data == nullptr || len < (int)sizeof(swarm_packet_t)) {
        return false;
    }

    uint16_t magic = 0;
    memcpy(&magic, data, sizeof(magic));
    if (magic != SWARM_MAGIC) {
        return false;
    }

    swarm_packet_t pkt = {};
    memcpy(&pkt, data, sizeof(pkt));

    if (pkt.source == (uint8_t)SELF_NODE_ID) {
        return true; /* our own broadcast looped back somehow; ignore */
    }

    mark_seen(pkt.source);

    switch ((swarm_msg_type_t)pkt.type) {
        case SWARM_MSG_HEARTBEAT:
            break;

        case SWARM_MSG_WAKE:
            ESP_LOGI(TAG, "Swarm: WAKE from node %u", pkt.source);
            break;

        case SWARM_MSG_FIRE_ALERT: {
            ESP_LOGW(TAG, "Swarm: FIRE_ALERT from node %u state=%u fire=%.1f%% smoke=%.1f%%",
                     pkt.source, pkt.payload.fire.state,
                     pkt.payload.fire.fire_confidence_x1000 / 10.0f,
                     pkt.payload.fire.smoke_confidence_x1000 / 10.0f);
            /* state is a bitmask: bit0=fire, bit1=smoke, bit2=gas (0x04) —
             * see WARDEN_HAZARD_GAS_BIT in warden_main.cpp. Build a short
             * combined label instead of a fixed lookup so any combination
             * (e.g. gas alone, fire+gas) renders sensibly. */
            char label[24] = {0};
            size_t pos = 0;
            if (pkt.payload.fire.state & 0x01) {
                pos += snprintf(label + pos, sizeof(label) - pos, "FIRE");
            }
            if (pkt.payload.fire.state & 0x02) {
                pos += snprintf(label + pos, sizeof(label) - pos, "%sSMOKE", pos ? "+" : "");
            }
            if (pkt.payload.fire.state & 0x04) {
                pos += snprintf(label + pos, sizeof(label) - pos, "%sGAS", pos ? "+" : "");
            }
            if (pos == 0) {
                snprintf(label, sizeof(label), "HAZARD");
            }
            snprintf(s_alert_text[pkt.source], sizeof(s_alert_text[pkt.source]), "%s", label);
            s_pending_hazard_bits = pkt.payload.fire.state;
            s_hazard_changed = true;
            break;
        }

        case SWARM_MSG_SECURITY_ALERT:
            ESP_LOGW(TAG, "Swarm: SECURITY_ALERT from node %u %s (%.1f%%)",
                     pkt.source, pkt.payload.security.label,
                     pkt.payload.security.confidence * 100.0f);
            snprintf(s_alert_text[pkt.source], sizeof(s_alert_text[pkt.source]), "%s",
                     pkt.payload.security.label[0] ? pkt.payload.security.label : "INTRUDER");
            break;

        case SWARM_MSG_MAP_UPDATE:
            ESP_LOGI(TAG, "Swarm: MAP_UPDATE from node %u (%.2f,%.2f) door=%d [%u/%u]",
                     pkt.source, pkt.payload.map.x, pkt.payload.map.y,
                     pkt.payload.map.has_door,
                     pkt.payload.map.snap_index + 1, pkt.payload.map.total_snaps);
            break;

        case SWARM_MSG_ALL_CLEAR:
            ESP_LOGI(TAG, "Swarm: ALL_CLEAR from node %u", pkt.source);
            s_alert_text[pkt.source][0] = '\0';
            s_pending_hazard_bits = 0;
            s_hazard_changed = true;
            break;

        default:
            ESP_LOGW(TAG, "Swarm: unknown type 0x%02X from node %u", pkt.type, pkt.source);
            break;
    }

    /* Do NOT touch the display from here: this function runs in the ESP-NOW
     * recv callback, which executes in the Wi-Fi driver task's own context.
     * bsp_display_lock() can block (or, if the LVGL/SPI path is ever wedged,
     * block indefinitely) — stalling the Wi-Fi driver task stalls *all*
     * Wi-Fi traffic, including HTTP telemetry to the backend, not just the
     * swarm link. Only record state here; heartbeat_timer_cb() (a much
     * lower-stakes esp_timer context) is the only place that touches LVGL. */
    s_online[pkt.source] = true;

    return true;
}

static void heartbeat_timer_cb(void *arg)
{
    (void)arg;
    send_packet(SWARM_MSG_HEARTBEAT, nullptr, 0);

    // Dispatch any hazard-alert/all-clear state recorded by
    // swarm_link_handle_frame() from the (unsafe-for-this-kind-of-work)
    // ESP-NOW recv callback — see the comment on s_hazard_changed above.
    if (s_hazard_changed) {
        s_hazard_changed = false;
        uint8_t bits = s_pending_hazard_bits;
        if (bits != 0) {
            if (s_hazard_alert_cb != nullptr) {
                s_hazard_alert_cb(bits);
            }
        } else if (s_all_clear_cb != nullptr) {
            s_all_clear_cb();
        }
    }

    int64_t t = now_ms();
    for (int node = 1; node <= 3; node++) {
        if (node == (int)SELF_NODE_ID) {
            continue;
        }
        bool is_online = (s_last_seen_ms[node] != 0) && (t - s_last_seen_ms[node] < 15000);
        if (s_online[node] != is_online) {
            s_online[node] = is_online;
            ESP_LOGI(TAG, "Swarm: node %d is now %s", node, is_online ? "ONLINE" : "OFFLINE");
        }
        /* Refresh every tick (not just on transition) so an alert_text change
         * recorded in swarm_link_handle_frame() reaches the screen within one
         * heartbeat period, without that function ever touching LVGL itself. */
        swarm_display_update((swarm_node_id_t)node, is_online, s_alert_text[node]);
    }
}

esp_err_t swarm_link_init(void)
{
    swarm_link_pin_channel();

    esp_err_t err;
    if ((err = ensure_peer(SWARM_MAC_PATHFINDER, AEGIS_SWARM_CHANNEL)) != ESP_OK) {
        ESP_LOGE(TAG, "add Pathfinder peer failed: %s", esp_err_to_name(err));
        return err;
    }
    if ((err = ensure_peer(SWARM_MAC_WARDEN, AEGIS_SWARM_CHANNEL)) != ESP_OK) {
        ESP_LOGE(TAG, "add Warden peer failed: %s", esp_err_to_name(err));
        return err;
    }
    if ((err = ensure_peer(s_broadcast_mac, AEGIS_SWARM_CHANNEL)) != ESP_OK) {
        ESP_LOGE(TAG, "add broadcast peer failed: %s", esp_err_to_name(err));
        return err;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &heartbeat_timer_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "swarm_hb",
    };
    if ((err = esp_timer_create(&timer_args, &s_heartbeat_timer)) != ESP_OK) {
        ESP_LOGE(TAG, "heartbeat timer create failed: %s", esp_err_to_name(err));
        return err;
    }
    if ((err = esp_timer_start_periodic(s_heartbeat_timer, 5000000)) != ESP_OK) {
        ESP_LOGE(TAG, "heartbeat timer start failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Swarm link ready on channel %d (Guardian)", AEGIS_SWARM_CHANNEL);
    return ESP_OK;
}
