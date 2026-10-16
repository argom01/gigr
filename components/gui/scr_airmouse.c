#include "app_req.h"
#include "gui_priv.h"

#define PAD_W 240
#define PAD_H 135
#define DOT 10

static lv_obj_t *s_status;
static lv_obj_t *s_warn;
static lv_obj_t *s_pad;
static lv_obj_t *s_dot;
static settings_t s_cfg;
static uint32_t s_cfg_version;

#define FLOAT_SETTER(name, field)        \
    static void name(float v)            \
    {                                    \
        GUI_SETTINGS_EDIT(s->field = v); \
    }

FLOAT_SETTER(on_gmin, am_gain_min)
FLOAT_SETTER(on_gmax, am_gain_max)
FLOAT_SETTER(on_v1, am_v1_dps)
FLOAT_SETTER(on_v2, am_v2_dps)
FLOAT_SETTER(on_cutoff, am_mincutoff_hz)
FLOAT_SETTER(on_beta, am_beta)
FLOAT_SETTER(on_dz, am_deadzone_dps)
FLOAT_SETTER(on_press, am_press_th)
FLOAT_SETTER(on_release, am_release_th)
FLOAT_SETTER(on_scroll, am_scroll_gain)

static void on_enable(bool v)
{
    GUI_SETTINGS_EDIT(s->am_enabled = v);
}

static void on_inv_x(bool v)
{
    GUI_SETTINGS_EDIT(s->am_invert_x = v);
}

static void on_inv_y(bool v)
{
    GUI_SETTINGS_EDIT(s->am_invert_y = v);
}

static void on_xsrc(int idx)
{
    GUI_SETTINGS_EDIT(s->am_x_source = (uint8_t)idx);
}

static void on_recentre(lv_event_t *e)
{
    (void)e;
    app_req_post(REQ_TEST_CURSOR_RESET);
}

static void on_defaults(lv_event_t *e)
{
    (void)e;
    settings_t d;
    settings_defaults(&d);
    GUI_SETTINGS_EDIT({
        s->am_x_source = d.am_x_source;
        s->am_invert_x = d.am_invert_x;
        s->am_invert_y = d.am_invert_y;
        s->am_gain_min = d.am_gain_min;
        s->am_gain_max = d.am_gain_max;
        s->am_v1_dps = d.am_v1_dps;
        s->am_v2_dps = d.am_v2_dps;
        s->am_mincutoff_hz = d.am_mincutoff_hz;
        s->am_beta = d.am_beta;
        s->am_deadzone_dps = d.am_deadzone_dps;
        s->am_press_th = d.am_press_th;
        s->am_release_th = d.am_release_th;
        s->am_scroll_gain = d.am_scroll_gain;
    });
}

void scr_airmouse_create(lv_obj_t *tab)
{
    settings_get(&s_cfg);
    s_cfg_version = settings_version();

    lv_obj_t *top = gui_section(tab, "Air mouse");
    gui_switch(top, "Enabled (curl ring + pinky to enter)", s_cfg.am_enabled, on_enable);
    s_warn = gui_label(top, "Flex calibration required before the air mouse can be entered");
    gui_set_text_color(s_warn, GUI_COL_WARN);
    s_status = gui_label(top, "");

    lv_obj_t *pad = gui_section(tab, "Test pad (works without a host)");
    s_pad = lv_obj_create(pad);
    lv_obj_set_size(s_pad, PAD_W, PAD_H);
    lv_obj_set_scrollable(s_pad, false);
    lv_obj_set_style_pad_all(s_pad, 0, 0);
    s_dot = lv_obj_create(s_pad);
    lv_obj_set_size(s_dot, DOT, DOT);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);
    lv_obj_set_style_bg_color(s_dot, GUI_COL_ACCENT, 0);
    gui_button(pad, "Re-centre", on_recentre, NULL);

    lv_obj_t *map = gui_section(tab, "Mapping");
    gui_dropdown(map, "Cursor X from", "Wrist yaw / arm\nForearm roll", s_cfg.am_x_source, on_xsrc);
    gui_switch(map, "Invert X", s_cfg.am_invert_x, on_inv_x);
    gui_switch(map, "Invert Y", s_cfg.am_invert_y, on_inv_y);

    lv_obj_t *gain = gui_section(tab, "Speed (slow = wrist precision, fast = arm sweeps)");
    gui_slider(gain, "Gain at slow speed px/deg", 1, 15, s_cfg.am_gain_min, 1, on_gmin);
    gui_slider(gain, "Gain at fast speed px/deg", 5, 60, s_cfg.am_gain_max, 1, on_gmax);
    gui_slider(gain, "Gain rises from deg/s", 1, 40, s_cfg.am_v1_dps, 0, on_v1);
    gui_slider(gain, "Gain maximum at deg/s", 40, 300, s_cfg.am_v2_dps, 0, on_v2);

    lv_obj_t *filt = gui_section(tab, "Filtering");
    gui_slider(filt, "1-euro min cut-off Hz", 0.1f, 5.0f, s_cfg.am_mincutoff_hz, 2, on_cutoff);
    gui_slider(filt, "1-euro beta", 0.0f, 0.2f, s_cfg.am_beta, 3, on_beta);
    gui_slider(filt, "Dead zone deg/s", 0.0f, 5.0f, s_cfg.am_deadzone_dps, 1, on_dz);

    lv_obj_t *clk = gui_section(tab, "Clicks (index = left, middle = right, both = scroll)");
    gui_slider(clk, "Press threshold", 0.3f, 0.9f, s_cfg.am_press_th, 2, on_press);
    gui_slider(clk, "Release threshold", 0.1f, 0.6f, s_cfg.am_release_th, 2, on_release);
    gui_slider(clk, "Scroll steps per degree", 0.02f, 0.5f, s_cfg.am_scroll_gain, 2, on_scroll);

    gui_button(tab, "Restore defaults (reopen tab to see)", on_defaults, NULL);
}

void scr_airmouse_update(const app_state_t *st)
{
    if (settings_version() != s_cfg_version) {
        s_cfg_version = settings_version();
        settings_get(&s_cfg);
    }
    lv_obj_set_hidden(s_warn, s_cfg.flex_cal_valid);

    const app_airmouse_state_t *a = &st->airmouse;
    const char *state = !a->active ? "inactive" : a->settling ? "settling" : a->frozen ? "frozen (click)"
                                                                         : a->scrolling ? "SCROLL" : "ACTIVE";
    lv_label_set_text_fmt(s_status, "%s   L:%s R:%s   speed %.0f deg/s  gain %.1f\nring %.2f  pinky %.2f  index %.2f  middle %.2f",
                          state, (a->buttons & 1) ? "DOWN" : "up", (a->buttons & 2) ? "DOWN" : "up",
                          (double)a->speed_dps, (double)a->gain, (double)st->sensor.flex_norm[FINGER_RING],
                          (double)st->sensor.flex_norm[FINGER_PINKY], (double)st->sensor.flex_norm[FINGER_INDEX],
                          (double)st->sensor.flex_norm[FINGER_MIDDLE]);
    gui_set_text_color(s_status, a->active ? GUI_COL_OK : GUI_COL_DIM);

    const int32_t x = a->test_x * (PAD_W - DOT) / 1920;
    const int32_t y = a->test_y * (PAD_H - DOT) / 1080;
    lv_obj_set_pos(s_dot, x, y);
    lv_obj_set_style_bg_color(s_dot, a->buttons ? GUI_COL_BAD : GUI_COL_ACCENT, 0);
}
