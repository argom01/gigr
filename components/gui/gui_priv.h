#ifndef GUI_PRIV_H
#define GUI_PRIV_H

#include <stdbool.h>
#include "app_state.h"
#include "lvgl.h"
#include "settings.h"

// ── Colours ──────────────────────────────────────────────────────────────────

#define GUI_COL_OK      lv_color_hex(0x4caf50)
#define GUI_COL_WARN    lv_color_hex(0xffb300)
#define GUI_COL_BAD     lv_color_hex(0xef5350)
#define GUI_COL_DIM     lv_color_hex(0x9e9e9e)
#define GUI_COL_ACCENT  lv_color_hex(0x42a5f5)

// ── Settings helper ──────────────────────────────────────────────────────────
// GUI_SETTINGS_EDIT(s->backlight_pct = 50);

#define GUI_SETTINGS_EDIT(stmt)        \
    do {                               \
        settings_t gui_s_;             \
        settings_get(&gui_s_);         \
        {                              \
            settings_t *s = &gui_s_;   \
            stmt;                      \
        }                              \
        settings_set(&gui_s_);         \
    } while (0)

// ── Widget helpers ───────────────────────────────────────────────────────────

typedef void (*gui_float_cb_t)(float value);
typedef void (*gui_bool_cb_t)(bool value);
typedef void (*gui_int_cb_t)(int value);

lv_obj_t *gui_column(lv_obj_t *parent);
lv_obj_t *gui_row(lv_obj_t *parent);
lv_obj_t *gui_section(lv_obj_t *parent, const char *title);
lv_obj_t *gui_label(lv_obj_t *parent, const char *text);
lv_obj_t *gui_title(lv_obj_t *parent, const char *text);
lv_obj_t *gui_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data);
lv_obj_t *gui_switch(lv_obj_t *parent, const char *text, bool state, gui_bool_cb_t cb);
lv_obj_t *gui_slider(lv_obj_t *parent, const char *text, float min, float max, float value, int decimals,
                     gui_float_cb_t cb);
void gui_slider_set(lv_obj_t *slider, float value);
lv_obj_t *gui_dropdown(lv_obj_t *parent, const char *text, const char *options, int selected, gui_int_cb_t cb);
lv_obj_t *gui_bar(lv_obj_t *parent, int32_t min, int32_t max);

void gui_set_text_color(lv_obj_t *obj, lv_color_t color);
const char *gui_hw_text(hw_status_t st);
lv_color_t gui_hw_color(hw_status_t st);

// ── Navigation ───────────────────────────────────────────────────────────────

lv_obj_t *gui_main_screen(void);
void gui_touchtest_open(void);

// ── Screens (one per tab) ────────────────────────────────────────────────────

typedef struct {
    const char *name;
    void (*create)(lv_obj_t *tab);
    void (*update)(const app_state_t *st);   // called at 20 Hz while the tab is visible
} gui_screen_t;

void scr_home_create(lv_obj_t *tab);
void scr_home_update(const app_state_t *st);
void scr_sensors_create(lv_obj_t *tab);
void scr_sensors_update(const app_state_t *st);
void scr_imu_create(lv_obj_t *tab);
void scr_imu_update(const app_state_t *st);
void scr_flex_create(lv_obj_t *tab);
void scr_flex_update(const app_state_t *st);
void scr_airmouse_create(lv_obj_t *tab);
void scr_airmouse_update(const app_state_t *st);
void scr_touchpad_create(lv_obj_t *tab);
lv_obj_t *scr_touchpad_screen(void);
void scr_touchpad_pad_update(const app_state_t *st);
void scr_system_create(lv_obj_t *tab);
void scr_system_update(const app_state_t *st);

#endif
