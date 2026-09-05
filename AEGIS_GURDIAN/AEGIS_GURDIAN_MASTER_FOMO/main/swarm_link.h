#pragma once

/*
 * AEGIS Swarm — Guardian's Master-to-Master ESP-NOW link.
 * See SWARM_ESPNOW_DESIGN.md at the repo root.
 *
 * This is a second, independent ESP-NOW peer set on AEGIS_SWARM_CHANNEL (6),
 * layered on top of the ESP-NOW stack Guardian's own espnow_master.cpp already
 * brings up for its Master<->Slave link. It does not touch that link.
 */

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call once, after espnow_master_init() has succeeded (ESP-NOW must already be
 * initialized). Registers the swarm peers (Pathfinder, Warden, broadcast) and
 * starts the heartbeat timer. */
esp_err_t swarm_link_init(void);

esp_err_t swarm_send_wake(void);
esp_err_t swarm_send_all_clear(void);
esp_err_t swarm_send_security_alert(bool is_intruder, bool is_dangerous_object,
                                     float confidence, const char *label);

/* Re-pins the Wi-Fi radio to AEGIS_SWARM_CHANNEL. Only takes effect while STA
 * is not associated to an AP (ESP-IDF locks the channel while associated) —
 * safe/expected to no-op otherwise. Call from WIFI_EVENT_STA_DISCONNECTED so
 * the swarm link keeps working through any AP outage. */
void swarm_link_pin_channel(void);

/* Registers a callback invoked on every SWARM_MSG_FIRE_ALERT received from
 * Warden, passed the raw hazard bitmask (bit0=fire, bit1=smoke, bit2=gas —
 * see WARDEN_HAZARD_GAS_BIT in Warden's warden_main.cpp). Guardian uses this
 * to prioritize its existing face-search loop (owner-finding). */
typedef void (*swarm_hazard_alert_cb_t)(uint8_t hazard_bits);
void swarm_link_set_hazard_alert_cb(swarm_hazard_alert_cb_t cb);

/* Registers a callback invoked once per SWARM_MSG_ALL_CLEAR received from any
 * sibling. Guardian uses this to end owner-search mode. */
typedef void (*swarm_all_clear_cb_t)(void);
void swarm_link_set_all_clear_cb(swarm_all_clear_cb_t cb);

/* Feeds a raw ESP-NOW receive frame to the swarm demux. Returns true if it was
 * a swarm packet (fully handled by this call) — caller should return without
 * doing anything else. Returns false if it wasn't swarm traffic at all, i.e.
 * the caller's own protocol should handle it as before. */
bool swarm_link_handle_frame(const uint8_t *data, int len);

#ifdef __cplusplus
}
#endif
