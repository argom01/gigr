#include <stdio.h>
#include "esp_timer.h"
#include "gesture.h"
#include "gui_priv.h"

static const char *const s_hw_names[HW_COUNT] = {
    [HW_DISPLAY] = "Display", [HW_TOUCH] = "Touch", [HW_IMU] = "IMU",
    [HW_FLEX] = "Flex",       [HW_BLE] = "BLE",     [HW_MODEL] = "Model",
};

static lv_obj_t *s_mode;
static lv_obj_t *s_ble;
static lv_obj_t *s_bat;
static lv_obj_t *s_gesture;
static lv_obj_t *s_hw[HW_COUNT];
static lv_obj_t *s_sw_air;
static lv_obj_t *s_sw_gest;

static void on_airmouse(bool v)
{
    GUI_SETTINGS_EDIT(s->am_enabled = v);
}

static void on_gestures(bool v)
{
    GUI_SETTINGS_EDIT(s->gesture_enabled = v);
}

void scr_home_create(lv_obj_t *tab)
{
    s_mode = gui_label(tab, "");
    lv_obj_set_style_text_font(s_mode, &lv_font_montserrat_28, 0);

    lv_obj_t *info = gui_section(tab, NULL);
    s_ble = gui_label(info, "");
    s_bat = gui_label(info, "");
    s_gesture = gui_label(info, "");

    settings_t cfg;
    settings_get(&cfg);
    lv_obj_t *sw = gui_section(tab, "Quick toggles");
    s_sw_gest = gui_switch(sw, "Gesture recognition", cfg.gesture_enabled, on_gestures);
    s_sw_air = gui_switch(sw, "Air mouse (curl ring + pinky)", cfg.am_enabled, on_airmouse);

    lv_obj_t *hw = gui_section(tab, "Hardware");
    lv_obj_t *row = gui_row(hw);
    for (int i = 0; i < HW_COUNT; i++) {
        s_hw[i] = gui_label(row, "");
        lv_obj_set_width(s_hw[i], 150);
    }
}

void scr_home_update(const app_state_t *st)
{
    lv_label_set_text(s_mode, app_mode_name(st->mode));

    if (st->ble.connected) {
        lv_label_set_text_fmt(s_ble, LV_SYMBOL_BLUETOOTH " Connected %02X:%02X:%02X:%02X:%02X:%02X", st->ble.peer[0],
                              st->ble.peer[1], st->ble.peer[2], st->ble.peer[3], st->ble.peer[4], st->ble.peer[5]);
        gui_set_text_color(s_ble, GUI_COL_OK);
    } else {
        lv_label_set_text(s_ble, st->ble.advertising ? LV_SYMBOL_BLUETOOTH " Advertising..." : LV_SYMBOL_BLUETOOTH " Off");
        gui_set_text_color(s_ble, GUI_COL_WARN);
    }

    if (st->battery.valid) {
        lv_label_set_text_fmt(s_bat, LV_SYMBOL_BATTERY_FULL " %u%%  %.2f V%s", st->battery.pct,
                              (double)(st->battery.mv / 1000.0f), st->battery.charging ? "  " LV_SYMBOL_CHARGE " charging" : "");
    } else {
        lv_label_set_text(s_bat, LV_SYMBOL_BATTERY_EMPTY " --");
    }

    if (st->gesture.last_id >= 0) {
        const float age = (float)(esp_timer_get_time() - st->gesture.last_t_us) / 1e6f;
        lv_label_set_text_fmt(s_gesture, "Last gesture: %s  p=%.2f  %.1f s ago", gesture_name(st->gesture.last_id),
                              (double)st->gesture.last_prob, (double)age);
    } else {
        lv_label_set_text(s_gesture, st->gesture.model_stub ? "Gesture model: stub (no trained model)" : "No gesture yet");
    }

    for (int i = 0; i < HW_COUNT; i++) {
        lv_label_set_text_fmt(s_hw[i], "%s: %s", s_hw_names[i], gui_hw_text(st->hw[i]));
        gui_set_text_color(s_hw[i], gui_hw_color(st->hw[i]));
    }
}
