#include <math.h>
#include <stdio.h>
#include "gui_priv.h"

static const char *const FINGER_NAMES[FINGER_COUNT] = { "Thumb", "Index", "Middle", "Ring", "Pinky" };

#define CHART_POINTS 120

static lv_obj_t *s_bar[FINGER_COUNT];
static lv_obj_t *s_lbl[FINGER_COUNT];
static lv_obj_t *s_cal_banner;
static lv_obj_t *s_imu;
static lv_obj_t *s_chart;
static lv_chart_series_t *s_ser[5];
static int s_chart_src;
static settings_t s_cfg;
static uint32_t s_cfg_version;

static const lv_color_t SERIES_COLORS[5] = {
    LV_COLOR_MAKE(0xef, 0x53, 0x50), LV_COLOR_MAKE(0x66, 0xbb, 0x6a), LV_COLOR_MAKE(0x42, 0xa5, 0xf5),
    LV_COLOR_MAKE(0xff, 0xca, 0x28), LV_COLOR_MAKE(0xab, 0x47, 0xbc),
};

static void setup_chart(void)
{
    static const int32_t ranges[3][2] = { { -500, 500 }, { -2000, 2000 }, { 0, 1000 } };
    lv_chart_set_axis_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, ranges[s_chart_src][0], ranges[s_chart_src][1]);
    const int n = s_chart_src == 2 ? 5 : 3;
    for (int i = 0; i < 5; i++) {
        lv_chart_set_all_values(s_chart, s_ser[i], LV_CHART_POINT_NONE);
        lv_chart_hide_series(s_chart, s_ser[i], i >= n);
    }
}

static void on_chart_src(int idx)
{
    s_chart_src = idx;
    setup_chart();
}

void scr_sensors_create(lv_obj_t *tab)
{
    s_cal_banner = gui_label(tab, "UNCALIBRATED - run the Flex calibration");
    gui_set_text_color(s_cal_banner, GUI_COL_WARN);

    lv_obj_t *flex = gui_section(tab, "Flex (0 = straight, 1 = bent)");
    for (int i = 0; i < FINGER_COUNT; i++) {
        s_lbl[i] = gui_label(flex, FINGER_NAMES[i]);
        lv_obj_set_style_text_font(s_lbl[i], &lv_font_montserrat_14, 0);
        s_bar[i] = gui_bar(flex, 0, 1000);
    }

    lv_obj_t *imu = gui_section(tab, "IMU (body frame)");
    s_imu = gui_label(imu, "");

    lv_obj_t *ch = gui_section(tab, NULL);
    gui_dropdown(ch, "Plot", "Gyro dps\nAccel mg\nFlex x1000", 0, on_chart_src);
    s_chart = lv_chart_create(ch);
    lv_obj_set_size(s_chart, LV_PCT(100), 160);
    lv_chart_set_type(s_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_chart, CHART_POINTS);
    lv_chart_set_update_mode(s_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(s_chart, 5, 0);
    lv_obj_set_style_size(s_chart, 0, 0, LV_PART_INDICATOR);
    for (int i = 0; i < 5; i++) {
        s_ser[i] = lv_chart_add_series(s_chart, SERIES_COLORS[i], LV_CHART_AXIS_PRIMARY_Y);
    }
    setup_chart();
}

void scr_sensors_update(const app_state_t *st)
{
    if (settings_version() != s_cfg_version) {
        s_cfg_version = settings_version();
        settings_get(&s_cfg);
    }
    lv_obj_set_hidden(s_cal_banner, s_cfg.flex_cal_valid);

    const app_sensor_state_t *s = &st->sensor;
    for (int i = 0; i < FINGER_COUNT; i++) {
        const float lo = fminf(s_cfg.flex_open_mv[i], s_cfg.flex_closed_mv[i]) - 50.0f;
        const float hi = fmaxf(s_cfg.flex_open_mv[i], s_cfg.flex_closed_mv[i]) + 50.0f;
        const bool out_of_range = s->flex_mv[i] < lo || s->flex_mv[i] > hi;
        lv_label_set_text_fmt(s_lbl[i], "%-6s %4.0f mV  n=%.2f   (open %.0f / closed %.0f)%s", FINGER_NAMES[i],
                              (double)s->flex_mv[i], (double)s->flex_norm[i], (double)s_cfg.flex_open_mv[i],
                              (double)s_cfg.flex_closed_mv[i], out_of_range ? "  OUT OF RANGE" : "");
        gui_set_text_color(s_lbl[i], out_of_range ? GUI_COL_WARN : lv_color_white());
        lv_bar_set_value(s_bar[i], (int32_t)(s->flex_norm[i] * 1000.0f), LV_ANIM_OFF);
    }

    lv_label_set_text_fmt(s_imu,
                          "acc  %+6.3f %+6.3f %+6.3f g\n"
                          "gyro %+7.1f %+7.1f %+7.1f dps\n"
                          "roll %+6.1f  pitch %+6.1f  yaw(rel) %+6.1f deg\n"
                          "%s  temp %.1f C",
                          (double)s->accel_g[0], (double)s->accel_g[1], (double)s->accel_g[2],
                          (double)s->gyro_dps[0], (double)s->gyro_dps[1], (double)s->gyro_dps[2],
                          (double)s->roll_deg, (double)s->pitch_deg, (double)s->yaw_deg, s->still ? "STILL" : "moving",
                          (double)s->temp_c);

    for (int i = 0; i < 5; i++) {
        int32_t v;
        if (s_chart_src == 0) {
            v = i < 3 ? (int32_t)s->gyro_dps[i] : 0;
        } else if (s_chart_src == 1) {
            v = i < 3 ? (int32_t)(s->accel_g[i] * 1000.0f) : 0;
        } else {
            v = (int32_t)(s->flex_norm[i] * 1000.0f);
        }
        lv_chart_set_next_value(s_chart, s_ser[i], v);
    }
}
