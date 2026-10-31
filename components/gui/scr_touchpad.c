#include "board_config.h"
#include "gui_priv.h"

// "Pad" tab (enter + settings) and the full-screen touchpad shown while in
// APP_MODE_TOUCHPAD. Touches never reach LVGL in that mode; they go to HID.

static lv_obj_t *s_pad_scr;
static lv_obj_t *s_pad_info;

static void on_start(lv_event_t *e)
{
    (void)e;
    app_mode_set(APP_MODE_TOUCHPAD);
}

static void on_gain(float v)
{
    GUI_SETTINGS_EDIT(s->touchpad_gain = v);
}

void scr_touchpad_create(lv_obj_t *tab)
{
    settings_t cfg;
    settings_get(&cfg);

    lv_obj_t *sec = gui_section(tab, "Remote touchpad");
    gui_label(sec, "1 finger: move    tap: left click\n"
                   "2 fingers: scroll    2-finger tap: right click\n"
                   "3-finger tap: middle click\n"
                   "Hold the top-right corner for 1 s to exit.");
    lv_obj_t *b = gui_button(sec, LV_SYMBOL_PLAY " Start touchpad", on_start, NULL);
    lv_obj_set_width(b, LV_PCT(100));
    gui_slider(sec, "Speed", 0.5f, 4.0f, cfg.touchpad_gain, 1, on_gain);

    // the full-screen pad itself
    s_pad_scr = lv_obj_create(NULL);
    lv_obj_set_scrollable(s_pad_scr, false);
    lv_obj_set_style_bg_color(s_pad_scr, lv_color_hex(0x101418), 0);

    lv_obj_t *corner = lv_obj_create(s_pad_scr);
    lv_obj_set_size(corner, TOUCHPAD_EXIT_CORNER_PX, TOUCHPAD_EXIT_CORNER_PX);
    lv_obj_align(corner, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_radius(corner, 0, 0);
    lv_obj_set_style_bg_color(corner, lv_color_hex(0x37474f), 0);
    lv_obj_set_scrollable(corner, false);
    lv_obj_t *x = gui_label(corner, LV_SYMBOL_CLOSE);
    lv_obj_center(x);

    s_pad_info = gui_label(s_pad_scr, "");
    lv_obj_set_style_text_color(s_pad_info, GUI_COL_DIM, 0);
    lv_obj_align(s_pad_info, LV_ALIGN_BOTTOM_MID, 0, -8);
}

lv_obj_t *scr_touchpad_screen(void)
{
    return s_pad_scr;
}

void scr_touchpad_pad_update(const app_state_t *st)
{
    lv_label_set_text_fmt(s_pad_info, "TOUCHPAD  %s  -  hold " LV_SYMBOL_CLOSE " 1 s to exit",
                          st->ble.connected ? "connected" : "NOT CONNECTED");
}
