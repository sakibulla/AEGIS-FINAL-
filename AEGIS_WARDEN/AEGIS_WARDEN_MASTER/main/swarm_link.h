#pragma once

/*
 * AEGIS Swarm — Warden's Master-to-Master ESP-NOW link.
 * See SWARM_ESPNOW_DESIGN.md at the repo root.
 *
 * Warden's existing ESP-NOW (espnow_init() in warden_main.cpp) is send-only
 * today — no recv callback is registered anywhere in the project. This module
 * registers the first one, purely additive: nothing existing is replaced.
 */

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call once, after espnow_init() has succeeded (ESP-NOW must already be
 * initialized). Registers the swarm peers (Guardian, Pathfinder, broadcast),
 * the recv callback, and starts the heartbeat timer. */
esp_err_t swarm_link_init(void);

esp_err_t swarm_send_fire_alert(uint8_t state, float fire_confidence, float smoke_confidence);
esp_err_t swarm_send_all_clear(void);

/* Re-pins the Wi-Fi radio to AEGIS_SWARM_CHANNEL. Only takes effect while STA
 * is not associated to an AP (ESP-IDF locks the channel while associated) —
 * safe/expected to no-op otherwise. Call from WIFI_EVENT_STA_DISCONNECTED so
 * the swarm link keeps working through any AP outage. */
void swarm_link_pin_channel(void);

/* Registers a callback invoked once per SWARM_MSG_WAKE received from Guardian.
 * See SWARM_ESPNOW_DESIGN.md §8a — arms a 5-minute scout-delay timer. Warden
 * has no existing patrol/movement state machine to gate on this today (its
 * fire/smoke inference loop runs continuously and intentionally is NOT gated
 * here — delaying detection would be a safety regression); the hook exists so
 * whoever adds Warden movement/patrol behavior later has it ready. */
typedef void (*swarm_wake_cb_t)(void);
void swarm_link_set_wake_cb(swarm_wake_cb_t cb);
void swarm_link_set_all_clear_cb(swarm_wake_cb_t cb);

/* Feeds a raw ESP-NOW receive frame to the swarm demux. Returns true if it was
 * a swarm packet (fully handled by this call). */
bool swarm_link_handle_frame(const uint8_t *data, int len);

/* Registers a callback for ESP-NOW frames that are NOT swarm packets — i.e.
 * everything from Warden's own Master<->Slave link (e.g. warden_slave_status_t
 * gas reports). swarm_link owns the one ESP-NOW recv callback slot this board
 * has, so this is how the Master<->Slave protocol handler (which never needed
 * its own recv callback before, being send-only) gets a look at incoming
 * frames without a second, conflicting esp_now_register_recv_cb() call. */
typedef void (*swarm_other_frame_cb_t)(const uint8_t *data, int len);
void swarm_link_set_other_frame_cb(swarm_other_frame_cb_t cb);

#ifdef __cplusplus
}
#endif
