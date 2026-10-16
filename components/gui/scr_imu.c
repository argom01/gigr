#include <math.h>
#include <stdio.h>
#include "app_req.h"
#include "gui_priv.h"

#define BUBBLE_D 130
#define DOT_D    18
#define BUBBLE_DEG 45.0f

static lv_obj_t *s_bubble;
static lv_obj_t *s_dot;
static lv_obj_t *s_angles;
static lv_obj_t *s_axis;
static lv_obj_t *s_cal;
static lv_obj_t *s_stats;

static void on_level(lv_event_t *e)
{
    (void)e;
    app_req_post(REQ_LEVEL);
}

static void on_level_reset(lv_event_t *e)
{
    (void)e;
    app_req_post(REQ_LEVEL_RESET);
}

static void on_gyro(lv_event_t *e)
{
    (void)e;
    app_req_post(REQ_GYRO_CAL);
}

static void on_fusion_reset(lv_event_t *e)
{
    (void)e;
    app_req_post(REQ_FUSION_RESET);
}

void scr_imu_create(lv_obj_t *tab)
{
    lv_obj_t *top = gui_row(tab);

    s_bubble = lv_obj_create(top);
    lv_obj_set_size(s_bubble, BUBBLE_D, BUBBLE_D);
    lv_obj_set_style_radius(s_bubble, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_scrollable(s_bubble, false);
    lv_obj_set_style_pad_all(s_bubble, 0, 0);
    lv_obj_t *ring = lv_obj_create(s_bubble);
    lv_obj_set_size(ring, DOT_D + 8, DOT_D + 8);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_center(ring);
    s_dot = lv_obj_create(s_bubble);
    lv_obj_set_size(s_dot, DOT_D, DOT_D);
    lv_obj_set_style_radius(s_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_dot, GUI_COL_OK, 0);
    lv_obj_set_style_border_width(s_dot, 0, 0);
    lv_obj_center(s_dot);

    lv_obj_t *col = lv_obj_create(top);
    lv_obj_remove_style_all(col);
    lv_obj_set_size(col, 220, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    s_angles = gui_label(col, "");
    lv_obj_set_style_text_font(s_angles, &lv_font_montserrat_20, 0);
    s_axis = gui_label(col, "");

    lv_obj_t *cal = gui_section(tab, "Calibration");
    gui_label(cal, "Level: hand flat, palm down, still for 1 s.\nGyro: glove resting still for 1 s.");
    lv_obj_t *row = gui_row(cal);
    gui_button(row, "Level", on_level, NULL);
    gui_button(row, "Clear level", on_level_reset, NULL);
    gui_button(row, "Gyro cal", on_gyro, NULL);
    gui_button(row, "Reset orient.", on_fusion_reset, NULL);
    s_cal = gui_label(cal, "");

    lv_obj_t *axes = gui_section(tab, "Axis check (expected)");
    gui_label(axes, "Palm down        -> +Z up\n"
                    "Fingertips down  -> -X up (i.e. +X down)\n"
                    "Thumb side up    -> +Y up (right hand)\n"
                    "Wrong? edit IMU_MOUNT_MATRIX in board_config.h");

    lv_obj_t *stats = gui_section(tab, "Loop");
    s_stats = gui_label(stats, "");
}

static const char *axis_up_text(const float up[3])
{
    static char buf[48];
    int best = 0;
    for (int i = 1; i < 3; i++) {
        if (fabsf(up[i]) > fabsf(up[best])) {
            best = i;
        }
    }
    snprintf(buf, sizeof(buf), "Up: %c%c  (%.2f)", up[best] >= 0 ? '+' : '-', "XYZ"[best], (double)fabsf(up[best]));
    return buf;
}

void scr_imu_update(const app_state_t *st)
{
    const app_sensor_state_t *s = &st->sensor;

    float dx = s->roll_deg / BUBBLE_DEG;
    float dy = -s->pitch_deg / BUBBLE_DEG;
    dx = fmaxf(-1.0f, fminf(1.0f, dx));
    dy = fmaxf(-1.0f, fminf(1.0f, dy));
    const int32_t r = (BUBBLE_D - DOT_D) / 2 - 4;
    lv_obj_align(s_dot, LV_ALIGN_CENTER, (int32_t)(dx * r), (int32_t)(dy * r));
    const bool level = fabsf(s->roll_deg) < 2.0f && fabsf(s->pitch_deg) < 2.0f;
    lv_obj_set_style_bg_color(s_dot, level ? GUI_COL_OK : GUI_COL_WARN, 0);

    lv_label_set_text_fmt(s_angles, "roll  %+6.1f\npitch %+6.1f\nyaw   %+6.1f", (double)s->roll_deg,
                          (double)s->pitch_deg, (double)s->yaw_deg);
    lv_label_set_text(s_axis, axis_up_text(s->up));

    static const char *const cal_txt[] = { "", "Gyro calibration: hold still...", "Levelling: hold flat and still...",
                                           "Done", "Failed: too much motion, try again" };
    lv_label_set_text(s_cal, s->imu_cal < 5 ? cal_txt[s->imu_cal] : "");
    gui_set_text_color(s_cal, s->imu_cal == IMUCAL_FAILED ? GUI_COL_BAD : GUI_COL_OK);

    lv_label_set_text_fmt(s_stats, "rate %.1f Hz  jitter %.2f ms  loop %.0f us  I2C errors %lu\nbias %+.2f %+.2f %+.2f dps  %s",
                          (double)s->rate_hz, (double)s->jitter_ms, (double)s->loop_us, (unsigned long)s->imu_errors,
                          (double)s->gyro_bias_dps[0], (double)s->gyro_bias_dps[1], (double)s->gyro_bias_dps[2],
                          s->still ? "still" : "moving");
}
