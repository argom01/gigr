#include <stdio.h>
#include "app_req.h"
#include "gui_priv.h"

static const char *const FINGER_NAMES[FINGER_COUNT] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };

static lv_obj_t *s_banner;
static lv_obj_t *s_status;
static lv_obj_t *s_result;
static settings_t s_cfg;
static uint32_t s_cfg_version;

static void on_step(lv_event_t *e)
{
    app_req_post((app_req_t)(intptr_t)lv_event_get_user_data(e));
}

void scr_flex_create(lv_obj_t *tab)
{
    s_banner = gui_label(tab, "UNCALIBRATED");
    gui_set_text_color(s_banner, GUI_COL_WARN);

    lv_obj_t *wiz = gui_section(tab, "Full calibration (in order, then Save)");
    gui_label(wiz, "Each capture waits 0.5 s, then records 1.5 s.\nKeep the hand still while it records.");
    lv_obj_t *row = gui_row(wiz);
    gui_button(row, "1 Open hand", on_step, (void *)(intptr_t)REQ_FLEX_CAL_OPEN);
    gui_button(row, "2 Fist", on_step, (void *)(intptr_t)REQ_FLEX_CAL_FIST);
    gui_button(row, "3 Thumb tucked", on_step, (void *)(intptr_t)REQ_FLEX_CAL_THUMB);
    gui_button(row, LV_SYMBOL_SAVE " Save", on_step, (void *)(intptr_t)REQ_FLEX_CAL_SAVE);

    lv_obj_t *quick = gui_section(tab, "After putting the glove back on");
    gui_label(quick, "Hand flat: moves the open point, keeps each span.");
    gui_button(quick, "Quick re-zero", on_step, (void *)(intptr_t)REQ_FLEX_REZERO);

    lv_obj_t *res = gui_section(tab, "Last capture");
    s_status = gui_label(res, "");
    s_result = gui_label(res, "");
}

void scr_flex_update(const app_state_t *st)
{
    if (settings_version() != s_cfg_version) {
        s_cfg_version = settings_version();
        settings_get(&s_cfg);
    }
    lv_obj_set_hidden(s_banner, s_cfg.flex_cal_valid);

    const app_flexcal_state_t *fc = &st->flexcal;
    static const char *const status_txt[] = { "Idle", "Get ready...", "Recording - hold still", "Captured",
                                              "Too noisy - hold still and repeat" };
    static const char *const step_txt[] = {
        [REQ_FLEX_CAL_OPEN] = "open hand", [REQ_FLEX_CAL_FIST] = "fist", [REQ_FLEX_CAL_THUMB] = "thumb tucked",
        [REQ_FLEX_REZERO] = "re-zero",
    };
    const char *step = fc->step < sizeof(step_txt) / sizeof(step_txt[0]) && step_txt[fc->step] ? step_txt[fc->step] : "";
    lv_label_set_text_fmt(s_status, "%s (%s)", status_txt[fc->status], step);
    gui_set_text_color(s_status, fc->status == FLEXCAL_NOISY ? GUI_COL_BAD : lv_color_white());

    char buf[400];
    size_t len = 0;
    for (int i = 0; i < FINGER_COUNT; i++) {
        const bool span = fc->span_ok_mask & (1u << i);
        const bool pol = fc->polarity_ok_mask & (1u << i);
        len += snprintf(buf + len, sizeof(buf) - len, "%-6s %6.0f +- %4.1f mV  span %s  %s\n", FINGER_NAMES[i],
                        (double)fc->mean_mv[i], (double)fc->std_mv[i], span ? "ok" : "SMALL",
                        pol ? "" : "(polarity reversed?)");
    }
    lv_label_set_text(s_result, buf);
}
