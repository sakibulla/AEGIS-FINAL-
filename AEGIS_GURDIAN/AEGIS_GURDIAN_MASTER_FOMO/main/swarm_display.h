#pragma once

/*
 * AEGIS Swarm — on-device status strip for the integrated ST7789 LCD.
 * See SWARM_ESPNOW_DESIGN.md §11.
 *
 * Since the swarm link is bot-to-bot only (no backend visibility), this is
 * the only place swarm state is observable. Draws a fixed 3-cell strip along
 * the bottom 40px of the 240x240 screen — Guardian | Pathfinder | Warden, in
 * that order on all three boards — showing each sibling's liveness and
 * current alert, if any.
 */

#include "esp_err.h"
#include "aegis_swarm_protocol.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Call once, after bsp_display_start() has succeeded. Draws the strip and the
 * three cells; the self cell is set once here and not touched afterwards. */
esp_err_t swarm_display_init(void);

/* Updates one sibling's cell. Safe to call from any task (takes the LVGL/BSP
 * display lock internally). alert_text NULL/empty shows "OK"; a non-empty
 * string replaces it (e.g. "FIRE", "INTRUDER"). online=false grays the cell
 * out regardless of alert_text. Calls for SELF_NODE's own id are ignored —
 * the self cell isn't driven by the swarm link's view of the sibling table. */
void swarm_display_update(swarm_node_id_t node, bool online, const char *alert_text);

/* Updates Guardian's own cell and folds it into the shared alert banner —
 * the self cell used to be hardcoded blue/"self" and never touched again, so
 * Guardian's own face/object detections never reached the screen even though
 * they were correctly detected and sent to the backend/swarm. Call this from
 * wherever local security state changes. alert_text NULL/empty clears the
 * alert (shows "OK"). */
void swarm_display_update_self(const char *alert_text);

#ifdef __cplusplus
}
#endif
