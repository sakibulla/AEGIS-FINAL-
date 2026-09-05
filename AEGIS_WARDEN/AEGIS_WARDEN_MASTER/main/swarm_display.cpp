#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "bsp/esp-bsp.h"
#include "lvgl.h"

#include "swarm_display.h"

static const char *TAG = "SWARM_DISPLAY";

static const swarm_node_id_t SELF_NODE_ID = SWARM_NODE_WARDEN;

static const char *NODE_NAME[4] = { "", "GUARDIAN", "PATHFINDER", "WARDEN" };

/* indexed by swarm_node_id_t (1..3); index 0 unused */
static lv_obj_t *s_cell[4] = { nullptr, nullptr, nullptr, nullptr };
static lv_obj_t *s_label[4] = { nullptr, nullptr, nullptr, nullptr };
static bool s_node_has_alert[4] = { false, false, false, false };
static char s_node_alert_text[4][32] = { {0}, {0}, {0}, {0} };

static lv_obj_t *s_banner = nullptr;
static lv_obj_t *s_banner_label = nullptr;

static lv_obj_t *create_cell(lv_obj_t *parent, swarm_node_id_t node)
{
    int idx = (int)node;

    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_remove_style_all(cell);
    lv_obj_set_size(cell, 80, 40);
    lv_obj_set_pos(cell, (idx - 1) * 80, 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cell, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, lv_color_black(), 0);
    lv_obj_set_style_pad_all(cell, 2, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(cell);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_set_width(label, LV_PCT(100));
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text_fmt(label, "%s\n...", NODE_NAME[idx]);

    s_cell[idx] = cell;
    s_label[idx] = label;
    return cell;
}

/* Scans siblings for the first active alert and updates the big banner
 * above the status strip. This — not the small 80x40 cells — is meant to be
 * the thing you actually notice from across a room. */
static void refresh_alert_banner(void)
{
    if (s_banner == nullptr) {
        return;
    }

    for (int node = 1; node <= 3; node++) {
        if (node == (int)SELF_NODE_ID) {
            continue;
        }
        if (s_node_has_alert[node]) {
            lv_obj_set_style_bg_color(s_banner, lv_palette_main(LV_PALETTE_RED), 0);
            lv_label_set_text_fmt(s_banner_label, "\xE2\x9A\xA0 %s: %s", NODE_NAME[node], s_node_alert_text[node]);
            return;
        }
    }

    lv_obj_set_style_bg_color(s_banner, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_label_set_text(s_banner_label, "SWARM OK");
}

esp_err_t swarm_display_init(void)
{
    if (!bsp_display_lock(1000)) {
        ESP_LOGW(TAG, "display lock timeout on init");
        return ESP_ERR_TIMEOUT;
    }

    lv_obj_t *scr = lv_screen_active();

    /* Alert banner: full width, large font, sits directly above the status
     * strip. This is the primary "notice this" surface — solid red with the
     * offending node + hazard when something's wrong, solid green otherwise.
     * The 3-cell strip below stays for per-node liveness detail. */
    s_banner = lv_obj_create(scr);
    lv_obj_remove_style_all(s_banner);
    lv_obj_set_size(s_banner, 240, 56);
    lv_obj_align(s_banner, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_set_style_bg_opa(s_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_banner, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_clear_flag(s_banner, LV_OBJ_FLAG_SCROLLABLE);

    s_banner_label = lv_label_create(s_banner);
    lv_obj_set_style_text_color(s_banner_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_banner_label, &lv_font_montserrat_24, 0);
    lv_obj_set_width(s_banner_label, LV_PCT(96));
    lv_obj_set_style_text_align(s_banner_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_banner_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_banner_label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(s_banner_label, "SWARM OK");

    lv_obj_t *strip = lv_obj_create(scr);
    lv_obj_remove_style_all(strip);
    lv_obj_set_size(strip, 240, 40);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    create_cell(strip, SWARM_NODE_GUARDIAN);
    create_cell(strip, SWARM_NODE_PATHFINDER);
    create_cell(strip, SWARM_NODE_WARDEN);

    /* Self is always "online" by definition — the board obviously knows it's
     * running. Not driven by swarm_display_update(), set once here. */
    lv_obj_set_style_bg_color(s_cell[SELF_NODE_ID], lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_label_set_text_fmt(s_label[SELF_NODE_ID], "%s\n(self)", NODE_NAME[SELF_NODE_ID]);

    bsp_display_unlock();

    ESP_LOGI(TAG, "Swarm display ready");
    return ESP_OK;
}

void swarm_display_update(swarm_node_id_t node, bool online, const char *alert_text)
{
    int idx = (int)node;
    if (idx < 1 || idx > 3 || idx == (int)SELF_NODE_ID || s_cell[idx] == nullptr) {
        return;
    }

    if (!bsp_display_lock(200)) {
        return; /* best-effort; skip this refresh rather than block the caller */
    }

    bool has_alert = online && (alert_text != nullptr && alert_text[0] != '\0');
    s_node_has_alert[idx] = has_alert;
    if (has_alert) {
        snprintf(s_node_alert_text[idx], sizeof(s_node_alert_text[idx]), "%s", alert_text);
    } else {
        s_node_alert_text[idx][0] = '\0';
    }

    lv_color_t bg;
    const char *status;
    if (!online) {
        bg = lv_palette_main(LV_PALETTE_GREY);
        status = "OFFLINE";
    } else if (has_alert) {
        bg = lv_palette_main(LV_PALETTE_RED);
        status = alert_text;
    } else {
        bg = lv_palette_main(LV_PALETTE_GREEN);
        status = "OK";
    }

    lv_obj_set_style_bg_color(s_cell[idx], bg, 0);
    lv_label_set_text_fmt(s_label[idx], "%s\n%s", NODE_NAME[idx], status);

    refresh_alert_banner();

    bsp_display_unlock();
}
