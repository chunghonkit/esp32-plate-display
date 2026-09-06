/**
 * LVGL v8 UI — License plate display (landscape 480x320)
 */
#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "ui";
static lv_obj_t *plate_label = NULL;
static lv_obj_t *status_dot = NULL;
static lv_obj_t *info_label = NULL;

#define PLATE_HOLD_MS       20000
#define PLATE_HOLD_CHECK_MS 200

static uint32_t    plate_shown_since = 0;
static bool        plate_auto_clear  = false;
static lv_timer_t *hold_timer        = NULL;

// Clears the plate label back to "---" once it has been shown for PLATE_HOLD_MS.
static void plate_hold_cb(lv_timer_t *timer) {
    if (!plate_auto_clear) return;
    if (lv_tick_get() - plate_shown_since >= PLATE_HOLD_MS) {
        plate_auto_clear = false;
        lv_label_set_text(plate_label, "---");
        ESP_LOGI(TAG, "Plate hold expired, cleared");
    }
}

void ui_init(void) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // Header bar (full width landscape)
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, 480, 45);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_make(0, 0, 31), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header_label = lv_label_create(header);
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_18, 0);
    lv_label_set_text(header_label, "BVS3 CARPARK ENTRY");
    lv_obj_set_style_text_color(header_label, lv_color_white(), 0);
    lv_obj_center(header_label);

    // Plate number (center)
    plate_label = lv_label_create(scr);
    lv_obj_set_style_text_font(plate_label, &lv_font_montserrat_48, 0);
    lv_label_set_text(plate_label, "---");
    lv_obj_set_style_text_color(plate_label, lv_color_make(0, 255, 255), 0);
    lv_obj_align(plate_label, LV_ALIGN_CENTER, 0, -10);

    // Info line
    info_label = lv_label_create(scr);
    lv_label_set_text(info_label, "");
    lv_obj_set_style_text_color(info_label, lv_color_make(255, 255, 255), 0);
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 40);

    // Status bar
    lv_obj_t *status_bar = lv_obj_create(scr);
    lv_obj_set_size(status_bar, 480, 28);
    lv_obj_align(status_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar, lv_color_make(0, 0, 31), 0);
    lv_obj_set_style_border_width(status_bar, 0, 0);
    lv_obj_set_style_radius(status_bar, 0, 0);
    lv_obj_set_style_pad_all(status_bar, 0, 0);
    lv_obj_clear_flag(status_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Status LED dot — green = MQTT connected, light grey = disconnected
    status_dot = lv_obj_create(status_bar);
    lv_obj_set_size(status_dot, 16, 16);
    lv_obj_set_style_radius(status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(status_dot, 0, 0);
    lv_obj_set_style_bg_color(status_dot, lv_color_make(211, 211, 211), 0);
    lv_obj_align(status_dot, LV_ALIGN_BOTTOM_RIGHT, -12, -6);

    ESP_LOGI(TAG, "UI initialized");

    // Periodic check that clears the plate after its 20 s hold window.
    hold_timer = lv_timer_create(plate_hold_cb, PLATE_HOLD_CHECK_MS, NULL);
}

// Detected plate from MQTT — shows for 20 s, auto-clears, refreshed by new detections.
void ui_set_plate(const char *plate) {
    lv_label_set_text(plate_label, plate);
    plate_shown_since = lv_tick_get();
    plate_auto_clear  = true;
    ESP_LOGI(TAG, "Plate: %s (hold 20s)", plate);
}

// Persistent message in the plate area (e.g. "Setup WiFi") — no auto-clear.
void ui_set_message(const char *text) {
    lv_label_set_text(plate_label, text);
    plate_auto_clear = false;
}

void ui_set_status(bool connected) {
    if (connected) {
        lv_obj_set_style_bg_color(status_dot, lv_color_make(0, 224, 0), 0);    // green
    } else {
        lv_obj_set_style_bg_color(status_dot, lv_color_make(211, 211, 211), 0); // light grey
    }
}

void ui_set_info(const char *text) {
    lv_label_set_text(info_label, text);
}
