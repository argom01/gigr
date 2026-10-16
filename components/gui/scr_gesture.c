#include <stdio.h>
#include "gesture.h"
#include "gesture_task.h"
#include "gui_priv.h"
#include "profiles.h"

static lv_obj_t *s_status;
static lv_obj_t *s_bar[GESTURE_COUNT];
static lv_obj_t *s_lbl[GESTURE_COUNT];
static lv_obj_t *s_map;
static lv_obj_t *s_profile_dd;
static lv_obj_t *s_test_dd;
static int s_shown_profile = -1;

static void on_enable(bool v)
{
    GUI_SETTINGS_EDIT(s->gesture_enabled = v);
}

static void on_th(float v)
{
    GUI_SETTINGS_EDIT(s->gesture_th_offset = v);
}

static void on_refr(float v)
{
    GUI_SETTINGS_EDIT(s->gesture_refractory_ms = (uint16_t)v);
}

static void on_profile(int idx)
{
    GUI_SETTINGS_EDIT(s->host_profile = (uint8_t)idx);
}

static void on_per_peer(bool v)
{
    GUI_SETTINGS_EDIT(s->profile_per_peer = v);
}

static void on_bind(lv_event_t *e)
{
    (void)e;
    gesture_bind_profile_to_peer();
}

static void on_fire(lv_event_t *e)
{
    (void)e;
    gesture_fire_test((int)lv_dropdown_get_selected(s_test_dd));
}

static void show_mapping(int profile)
{
    char buf[600];
    size_t n = (size_t)snprintf(buf, sizeof(buf), "Active profile: %s\n", g_profiles[profile].name);
    for (int i = 0; i < GESTURE_COUNT && n < sizeof(buf); i++) {
        if (g_gestures[i].kind == G_KIND_IDLE) {
            continue;
        }
        char desc[40];
        action_describe(profile_action(profile, i), desc, sizeof(desc));
        n += (size_t)snprintf(buf + n, sizeof(buf) - n, "  %-12s -> %s\n", gesture_name(i), desc);
    }
    lv_label_set_text(s_map, buf);
    s_shown_profile = profile;
}

void scr_gesture_create(lv_obj_t *tab)
{
    settings_t cfg;
    settings_get(&cfg);

    lv_obj_t *top = gui_section(tab, "Gesture recognition");
    gui_switch(top, "Enabled", cfg.gesture_enabled, on_enable);
    s_status = gui_label(top, "");

    lv_obj_t *sc = gui_section(tab, "Live scores");
    for (int i = 0; i < GESTURE_COUNT; i++) {
        s_lbl[i] = gui_label(sc, gesture_name(i));
        s_bar[i] = gui_bar(sc, 0, 100);
    }

    lv_obj_t *tune = gui_section(tab, "Decision");
    gui_slider(tune, "Threshold offset", -0.2f, 0.15f, cfg.gesture_th_offset, 2, on_th);
    gui_slider(tune, "Refractory ms", 200, 1500, cfg.gesture_refractory_ms, 0, on_refr);

    lv_obj_t *prof = gui_section(tab, "Host profile");
    s_profile_dd = gui_dropdown(prof, "Default", profile_dropdown_options(), cfg.host_profile, on_profile);
    gui_switch(prof, "Remember a profile per paired host", cfg.profile_per_peer, on_per_peer);
    gui_button(prof, "Use default profile for this host", on_bind, NULL);
    s_map = gui_label(prof, "");
    lv_obj_set_style_text_font(s_map, &lv_font_montserrat_14, 0);

    lv_obj_t *test = gui_section(tab, "Test (sends the mapped action, no model needed)");
    lv_obj_t *row = gui_row(test);
    s_test_dd = lv_dropdown_create(row);
    char opts[256];
    size_t n = 0;
    for (int i = 0; i < GESTURE_COUNT; i++) {
        n += (size_t)snprintf(opts + n, sizeof(opts) - n, "%s%s", i ? "\n" : "", gesture_name(i));
    }
    lv_dropdown_set_options(s_test_dd, opts);
    lv_dropdown_set_selected(s_test_dd, GESTURE_COUNT > 1 ? 1 : 0);
    lv_obj_set_width(s_test_dd, 200);
    gui_button(row, LV_SYMBOL_PLAY " Fire", on_fire, NULL);
}

void scr_gesture_update(const app_state_t *st)
{
    const app_gesture_state_t *g = &st->gesture;
    if (g->model_stub) {
        lv_label_set_text(s_status, "No Edge Impulse model built in (stub). Test buttons still work.");
        gui_set_text_color(s_status, GUI_COL_WARN);
    } else if (g->model_mismatch) {
        lv_label_set_text(s_status, "Model window/axes do not match the firmware - see log");
        gui_set_text_color(s_status, GUI_COL_BAD);
    } else {
        lv_label_set_text_fmt(s_status, "%u labels matched, inference %lu us, fired %lu%s", g->matched_labels,
                              (unsigned long)g->infer_us, (unsigned long)g->fired,
                              st->mode == APP_MODE_GESTURE ? "" : "  (paused: not in gesture mode)");
        gui_set_text_color(s_status, GUI_COL_OK);
    }

    for (int i = 0; i < GESTURE_COUNT; i++) {
        lv_bar_set_value(s_bar[i], (int32_t)(g->scores[i] * 100.0f), LV_ANIM_OFF);
        const bool last = g->last_id == i;
        lv_label_set_text_fmt(s_lbl[i], "%s %.2f%s", gesture_name(i), (double)g->scores[i], last ? "  <- last" : "");
    }
    if (g->profile != s_shown_profile && g->profile < g_profile_count) {
        show_mapping(g->profile);
    }
}
