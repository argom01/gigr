#include "gui.h"

#include <stdio.h>
#include "display.h"
#include "esp_log.h"
#include "gui_priv.h"

static const char *TAG = "gui";

#define GUI_UPDATE_MS 50

static const gui_screen_t s_screens[] = {
    { LV_SYMBOL_HOME " Home", scr_home_create, scr_home_update },
    { LV_SYMBOL_SETTINGS " Sys", scr_system_create, scr_system_update },
};
#define SCREEN_COUNT (sizeof(s_screens) / sizeof(s_screens[0]))

static lv_obj_t *s_main_screen;
static lv_obj_t *s_tabview;
static lv_obj_t *s_status;
static app_state_t s_state;

static void update_status(const app_state_t *st)
{
    char bat[16] = "";
    if (st->battery.valid) {
        snprintf(bat, sizeof(bat), "%s%u%%", st->battery.charging ? LV_SYMBOL_CHARGE : "", st->battery.pct);
    }
    lv_label_set_text_fmt(s_status, "%s %s  %s", st->ble.connected ? LV_SYMBOL_BLUETOOTH : "-", bat,
                          app_mode_name(st->mode));
    gui_set_text_color(s_status, st->ble.connected ? GUI_COL_ACCENT : GUI_COL_DIM);
}

static void update_timer_cb(lv_timer_t *t)
{
    (void)t;
    app_state_get(&s_state);
    update_status(&s_state);

    const uint32_t active = lv_tabview_get_tab_active(s_tabview);
    if (lv_screen_active() == s_main_screen && active < SCREEN_COUNT && s_screens[active].update) {
        s_screens[active].update(&s_state);
    }
}

void gui_init(void)
{
    s_main_screen = lv_screen_active();

    s_tabview = lv_tabview_create(s_main_screen);
    lv_tabview_set_tab_bar_position(s_tabview, LV_DIR_LEFT);
    lv_tabview_set_tab_bar_size(s_tabview, 84);
    lv_obj_t *bar = lv_tabview_get_tab_bar(s_tabview);
    lv_obj_set_style_text_font(bar, &lv_font_montserrat_14, 0);

    for (size_t i = 0; i < SCREEN_COUNT; i++) {
        lv_obj_t *tab = lv_tabview_add_tab(s_tabview, s_screens[i].name);
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(tab, 6, 0);
        lv_obj_set_style_pad_row(tab, 6, 0);
        s_screens[i].create(tab);
    }

    // Small status line, always visible in the top-right corner
    s_status = lv_label_create(lv_layer_top());
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_14, 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_RIGHT, -6, 2);
    lv_label_set_text(s_status, "");

    settings_t cfg;
    settings_get(&cfg);
    if (cfg.perf_overlay) {
        lv_sysmon_show_performance(display_get());
    } else {
        lv_sysmon_hide_performance(display_get());
    }

    lv_timer_create(update_timer_cb, GUI_UPDATE_MS, NULL);
    ESP_LOGI(TAG, "GUI created (%u screens)", (unsigned)SCREEN_COUNT);
}
