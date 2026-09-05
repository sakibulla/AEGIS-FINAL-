#pragma once

/*
 * AEGIS Swarm — Pathfinder's Master-to-Master ESP-NOW link.
 * See SWARM_ESPNOW_DESIGN.md at the repo root.
 *
 * This is a second, independent ESP-NOW peer set on AEGIS_SWARM_CHANNEL (6),
 * layered on top of the ESP-NOW stack Pathfinder's own espnow_mesh.cpp already
 * brings up for its Master<->Slave link. It does not touch that link — it
 * only retunes the pre-existing broadcast peer's channel (see swarm_link.cpp)
 * since ESP-NOW keys peers by MAC and that broadcast peer already exists.
 */

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call once, after init_master_espnow() has succeeded (ESP-NOW must already be
 * initialized). Registers the swarm peers (Guardian, Warden, broadcast) and
 * starts the heartbeat timer. */
esp_err_t swarm_link_init(void);

esp_err_t swarm_send_map_update(float x, float y, bool has_door, uint8_t snap_index, uint8_t total_snaps);

/* Re-pins the Wi-Fi radio to AEGIS_SWARM_CHANNEL. Only takes effect while STA
 * is not associated to an AP (ESP-IDF locks the channel while associated) —
 * safe/expected to no-op otherwise. Call from WIFI_EVENT_STA_DISCONNECTED so
 * the swarm link keeps working through any AP outage. */
void swarm_link_pin_channel(void);

/* Registers a callback invoked once per SWARM_MSG_WAKE received from Guardian.
 * Pathfinder uses this to arm the 5-minute scout-delay timer before starting
 * pathfinder_mapping_task's Phase 1 (see SWARM_ESPNOW_DESIGN.md §8a). */
typedef void (*swarm_wake_cb_t)(void);
void swarm_link_set_wake_cb(swarm_wake_cb_t cb);

/* Registers a callback invoked once per SWARM_MSG_ALL_CLEAR received from any
 * sibling. Pathfinder uses this to cancel a pending scout-delay timer if the
 * alert that triggered WAKE clears before the 5 minutes are up (§8a), and to
 * end an active evacuation (see swarm_link_set_hazard_alert_cb below). */
void swarm_link_set_all_clear_cb(swarm_wake_cb_t cb);

/* Registers a callback invoked on every SWARM_MSG_FIRE_ALERT received from
 * Warden, passed the raw hazard bitmask (bit0=fire, bit1=smoke, bit2=gas —
 * see WARDEN_HAZARD_GAS_BIT in Warden's warden_main.cpp). Pathfinder uses
 * this to divert toward a known exit door. */
typedef void (*swarm_hazard_alert_cb_t)(uint8_t hazard_bits);
void swarm_link_set_hazard_alert_cb(swarm_hazard_alert_cb_t cb);

/* Feeds a raw ESP-NOW receive frame to the swarm demux. Returns true if it was
 * a swarm packet (fully handled by this call) — caller should return without
 * doing anything else. Returns false if it wasn't swarm traffic at all, i.e.
 * the caller's own protocol (data[0] == msg_type) should handle it as before. */
bool swarm_link_handle_frame(const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif
