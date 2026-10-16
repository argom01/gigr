#include "sensor_task.h"

#include <math.h>
#include <string.h>
#include "app_req.h"
#include "app_state.h"
#include "battery.h"
#include "board_config.h"
#include "board_io.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "flex_adc.h"
#include "flex_cal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gyro_bias.h"
#include "imu.h"
#include "mahony.h"
#include "quat.h"
#include "settings.h"
#include "task_config.h"
#include "vec3.h"

static const char *TAG = "sensor";

#define PERIOD_MS       (1000 / IMU_SAMPLE_HZ)
#define DT_NOMINAL      (1.0f / IMU_SAMPLE_HZ)
#define IMU_CAL_SAMPLES IMU_SAMPLE_HZ           // 1 s of data
#define IMU_CAL_TIMEOUT_US (6 * 1000 * 1000)

typedef struct {
    // configuration
    settings_t cfg;
    uint32_t cfg_version;
    float mount[3][3];
    bool imu_ok;
    bool flex_ok;

    // IMU pipeline
    gyro_bias_t bias;
    mahony_t ahrs;
    float yaw_offset;

    // IMU calibrations
    imucal_status_t imu_cal;
    int64_t imu_cal_start_us;
    float level_sum[3];
    int level_n;

    // flex
    float flex_lpf[FINGER_COUNT];
    bool flex_lpf_init;

    // flex calibration
    app_flexcal_state_t fc;
    int64_t fc_t0_us;
    welford_t fc_stats[FINGER_COUNT];
    float pending_open[FINGER_COUNT];
    float pending_closed[FINGER_COUNT];

    // battery
    battery_est_t bat;
    float bat_sum;
    int bat_n;

    // loop statistics
    int64_t last_us;
    float worst_dev_ms;
    int stat_n;
    int64_t stat_t0_us;
} sensor_ctx_t;

static sensor_ctx_t s_ctx;

// ── Configuration ────────────────────────────────────────────────────────────

static void reload_settings(sensor_ctx_t *c)
{
    if (settings_version() == c->cfg_version) {
        return;
    }
    c->cfg_version = settings_version();
    settings_get(&c->cfg);
}

static void save_settings(sensor_ctx_t *c)
{
    settings_set(&c->cfg);
    c->cfg_version = settings_version();
}

// ── Flex calibration state machine ───────────────────────────────────────────

static void flexcal_start(sensor_ctx_t *c, app_req_t step, int64_t now)
{
    c->fc.status = FLEXCAL_SETTLING;
    c->fc.step = (uint8_t)step;
    c->fc_t0_us = now;
    for (int i = 0; i < FINGER_COUNT; i++) {
        welford_reset(&c->fc_stats[i]);
    }
}

static void flexcal_finish(sensor_ctx_t *c)
{
    bool noisy = false;
    for (int i = 0; i < FINGER_COUNT; i++) {
        c->fc.mean_mv[i] = c->fc_stats[i].mean;
        c->fc.std_mv[i] = welford_std(&c->fc_stats[i]);
        if (c->fc.std_mv[i] > FLEX_CAL_MAX_STD_MV) {
            noisy = true;
        }
    }
    if (noisy) {
        c->fc.status = FLEXCAL_NOISY;
        ESP_LOGW(TAG, "flex capture too noisy, repeat");
        return;
    }

    switch ((app_req_t)c->fc.step) {
    case REQ_FLEX_CAL_OPEN:
        memcpy(c->pending_open, c->fc.mean_mv, sizeof(c->pending_open));
        break;
    case REQ_FLEX_CAL_FIST:
        for (int i = FINGER_INDEX; i < FINGER_COUNT; i++) {
            c->pending_closed[i] = c->fc.mean_mv[i];
        }
        break;
    case REQ_FLEX_CAL_THUMB:
        c->pending_closed[FINGER_THUMB] = c->fc.mean_mv[FINGER_THUMB];
        break;
    case REQ_FLEX_REZERO:
        // keep each finger's span, move the open point
        for (int i = 0; i < FINGER_COUNT; i++) {
            const float span = c->cfg.flex_closed_mv[i] - c->cfg.flex_open_mv[i];
            c->cfg.flex_open_mv[i] = c->fc.mean_mv[i];
            c->cfg.flex_closed_mv[i] = c->fc.mean_mv[i] + span;
        }
        save_settings(c);
        ESP_LOGI(TAG, "flex re-zeroed");
        break;
    default:
        break;
    }
    c->fc.span_ok_mask = flex_cal_span_mask(c->pending_open, c->pending_closed, FLEX_CAL_MIN_SPAN_MV);
    c->fc.polarity_ok_mask = flex_cal_polarity_mask(c->pending_open, c->pending_closed);
    c->fc.status = FLEXCAL_DONE;
}

static void flexcal_save(sensor_ctx_t *c)
{
    const uint8_t ok = flex_cal_span_mask(c->pending_open, c->pending_closed, FLEX_CAL_MIN_SPAN_MV);
    for (int i = 0; i < FINGER_COUNT; i++) {
        if (ok & (1u << i)) {
            c->cfg.flex_open_mv[i] = c->pending_open[i];
            c->cfg.flex_closed_mv[i] = c->pending_closed[i];
        } else {
            ESP_LOGW(TAG, "finger %d: span too small, keeping previous calibration", i);
        }
    }
    c->cfg.flex_cal_valid = (ok == FLEX_ALL_FINGERS_MASK);
    save_settings(c);
    c->fc.span_ok_mask = ok;
    c->fc.polarity_ok_mask = flex_cal_polarity_mask(c->pending_open, c->pending_closed);
    ESP_LOGI(TAG, "flex calibration saved (mask 0x%02x)", ok);
}

static void flexcal_step(sensor_ctx_t *c, const float mv[FINGER_COUNT], int64_t now)
{
    static const app_req_t steps[] = { REQ_FLEX_CAL_OPEN, REQ_FLEX_CAL_FIST, REQ_FLEX_CAL_THUMB, REQ_FLEX_REZERO };
    for (size_t i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        if (app_req_take(steps[i])) {
            flexcal_start(c, steps[i], now);
        }
    }
    if (app_req_take(REQ_FLEX_CAL_SAVE)) {
        flexcal_save(c);
    }

    const int64_t elapsed_ms = (now - c->fc_t0_us) / 1000;
    if (c->fc.status == FLEXCAL_SETTLING && elapsed_ms >= FLEX_CAL_SETTLE_MS) {
        c->fc.status = FLEXCAL_CAPTURING;
    } else if (c->fc.status == FLEXCAL_CAPTURING) {
        for (int i = 0; i < FINGER_COUNT; i++) {
            welford_add(&c->fc_stats[i], mv[i]);
        }
        if (elapsed_ms >= FLEX_CAL_SETTLE_MS + FLEX_CAL_CAPTURE_MS) {
            flexcal_finish(c);
        }
    }
}

// ── IMU calibrations (gyro bias, levelling) ─────────────────────────────────

static void imucal_step(sensor_ctx_t *c, const float a_mounted[3], const float g_raw[3], int64_t now)
{
    if (app_req_take(REQ_GYRO_CAL)) {
        gyro_bias_capture_start(&c->bias, IMU_CAL_SAMPLES);
        c->imu_cal = IMUCAL_GYRO_BUSY;
        c->imu_cal_start_us = now;
    }
    if (app_req_take(REQ_LEVEL)) {
        c->level_sum[0] = c->level_sum[1] = c->level_sum[2] = 0.0f;
        c->level_n = 0;
        c->imu_cal = IMUCAL_LEVEL_BUSY;
        c->imu_cal_start_us = now;
    }
    if (app_req_take(REQ_LEVEL_RESET)) {
        quat_identity(c->cfg.level_quat);
        c->cfg.level_valid = false;
        save_settings(c);
        ESP_LOGI(TAG, "levelling cleared");
    }

    if (c->imu_cal == IMUCAL_GYRO_BUSY) {
        if (gyro_bias_capture_poll(&c->bias)) {
            memcpy(c->cfg.gyro_bias_dps, c->bias.bias, sizeof(c->cfg.gyro_bias_dps));
            c->cfg.gyro_bias_valid = true;
            save_settings(c);
            c->imu_cal = IMUCAL_DONE;
            ESP_LOGI(TAG, "gyro bias %.2f %.2f %.2f dps", (double)c->bias.bias[0], (double)c->bias.bias[1],
                     (double)c->bias.bias[2]);
        } else if (now - c->imu_cal_start_us > IMU_CAL_TIMEOUT_US) {
            c->bias.capturing = false;
            c->imu_cal = IMUCAL_FAILED;
        }
    } else if (c->imu_cal == IMUCAL_LEVEL_BUSY) {
        const float g_norm = sqrtf(g_raw[0] * g_raw[0] + g_raw[1] * g_raw[1] + g_raw[2] * g_raw[2]);
        const float a_norm = v3_norm(a_mounted);
        if (g_norm - v3_norm(c->bias.bias) > 4.0f || fabsf(a_norm - 1.0f) > 0.1f) {
            c->level_n = 0;     // moving: restart the average
            c->level_sum[0] = c->level_sum[1] = c->level_sum[2] = 0.0f;
        } else {
            for (int i = 0; i < 3; i++) {
                c->level_sum[i] += a_mounted[i];
            }
            c->level_n++;
        }
        if (c->level_n >= IMU_CAL_SAMPLES) {
            float g[3] = { c->level_sum[0], c->level_sum[1], c->level_sum[2] };
            v3_normalize(g);
            const float up[3] = { 0.0f, 0.0f, 1.0f };
            quat_from_two_vectors(g, up, c->cfg.level_quat);
            c->cfg.level_valid = true;
            save_settings(c);
            mahony_reset(&c->ahrs);
            c->imu_cal = IMUCAL_DONE;
            ESP_LOGI(TAG, "levelled: gravity was %.3f %.3f %.3f", (double)g[0], (double)g[1], (double)g[2]);
        } else if (now - c->imu_cal_start_us > IMU_CAL_TIMEOUT_US) {
            c->imu_cal = IMUCAL_FAILED;
        }
    }
}

// ── Main loop ────────────────────────────────────────────────────────────────

static void update_stats(sensor_ctx_t *c, app_sensor_state_t *out, int64_t now)
{
    if (c->last_us) {
        const float dev = fabsf((float)(now - c->last_us) / 1000.0f - PERIOD_MS);
        if (dev > c->worst_dev_ms) {
            c->worst_dev_ms = dev;
        }
    }
    c->last_us = now;
    c->stat_n++;
    if (now - c->stat_t0_us >= 1000000) {
        out->rate_hz = (float)c->stat_n * 1e6f / (float)(now - c->stat_t0_us);
        out->jitter_ms = c->worst_dev_ms;
        c->stat_n = 0;
        c->worst_dev_ms = 0.0f;
        c->stat_t0_us = now;
    }
}

static void sensor_task(void *arg)
{
    sensor_ctx_t *c = arg;
    app_sensor_state_t st = { 0 };
    TickType_t wake = xTaskGetTickCount();
    uint32_t tick = 0;

    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(PERIOD_MS));
        const int64_t now = esp_timer_get_time();
        const float dt = c->last_us ? (float)(now - c->last_us) * 1e-6f : DT_NOMINAL;
        update_stats(c, &st, now);
        reload_settings(c);
        tick++;

        // ── IMU ──
        if (c->imu_ok) {
            float a_raw[3], g_raw[3];
            if (imu_read(a_raw, g_raw, &st.temp_c) == ESP_OK) {
                st.still = gyro_bias_update(&c->bias, g_raw, a_raw, dt);
                const float g_corr[3] = { g_raw[0] - c->bias.bias[0], g_raw[1] - c->bias.bias[1],
                                          g_raw[2] - c->bias.bias[2] };

                float a_m[3], g_m[3];
                m3_mul_v3(c->mount, a_raw, a_m);
                m3_mul_v3(c->mount, g_corr, g_m);
                imucal_step(c, a_m, g_raw, now);

                quat_rotate(c->cfg.level_quat, a_m, st.accel_g);
                quat_rotate(c->cfg.level_quat, g_m, st.gyro_dps);

                const float g_rad[3] = { DEG2RAD(st.gyro_dps[0]), DEG2RAD(st.gyro_dps[1]), DEG2RAD(st.gyro_dps[2]) };
                if (app_req_take(REQ_FUSION_RESET)) {
                    mahony_reset(&c->ahrs);
                    c->yaw_offset = 0.0f;
                }
                mahony_update(&c->ahrs, g_rad, st.accel_g, dt);
                memcpy(st.quat, c->ahrs.q, sizeof(st.quat));
                quat_up_in_body(c->ahrs.q, st.up);
                quat_to_euler_deg(c->ahrs.q, &st.roll_deg, &st.pitch_deg, &st.yaw_deg);
                memcpy(st.gyro_bias_dps, c->bias.bias, sizeof(st.gyro_bias_dps));
            } else {
                st.imu_errors++;
            }
            st.imu_cal = (uint8_t)c->imu_cal;
        }

        // ── Flex + battery ──
        if (c->flex_ok) {
            float mv[FINGER_COUNT], bat_adc_mv;
            flex_adc_poll(mv, &bat_adc_mv);
            const float k = dt / (1.0f / (2.0f * (float)M_PI * FLEX_LPF_HZ) + dt);
            for (int i = 0; i < FINGER_COUNT; i++) {
                const float n = flex_normalize_finger(i, mv[i], c->cfg.flex_open_mv[i], c->cfg.flex_closed_mv[i]);
                c->flex_lpf[i] = c->flex_lpf_init ? c->flex_lpf[i] + k * (n - c->flex_lpf[i]) : n;
                st.flex_mv[i] = mv[i];
                st.flex_norm[i] = c->flex_lpf[i];
            }
            c->flex_lpf_init = true;
            flexcal_step(c, mv, now);

            c->bat_sum += bat_adc_mv;
            c->bat_n++;
            if (c->bat_n >= IMU_SAMPLE_HZ) {
                const bool charging = board_is_charging();
                const float bat_mv = c->bat_sum / (float)c->bat_n * BAT_DIVIDER_RATIO;
                app_battery_state_t bat = {
                    .mv = (uint16_t)bat_mv,
                    .pct = battery_est_update(&c->bat, bat_mv, charging, 1.0f),
                    .charging = charging,
                    .valid = true,
                };
                APP_STATE_PUT(battery, &bat);
                c->bat_sum = 0.0f;
                c->bat_n = 0;
            }
            APP_STATE_PUT(flexcal, &c->fc);
        }

        st.t_us = now;
        st.loop_us = (float)(esp_timer_get_time() - now);
        APP_STATE_PUT(sensor, &st);
    }
}

esp_err_t sensor_task_start(void)
{
    sensor_ctx_t *c = &s_ctx;
    memset(c, 0, sizeof(*c));

    settings_get(&c->cfg);
    c->cfg_version = settings_version();
    if (!c->cfg.level_valid) {
        quat_identity(c->cfg.level_quat);
    }
    const float mount[3][3] = IMU_MOUNT_MATRIX;
    memcpy(c->mount, mount, sizeof(mount));

    gyro_bias_init(&c->bias, IMU_STILL_GYRO_DPS, IMU_STILL_ACCEL_G, IMU_STILL_TIME_S, IMU_BIAS_TRACK_TAU_S);
    if (c->cfg.gyro_bias_valid) {
        gyro_bias_set(&c->bias, c->cfg.gyro_bias_dps);
    }
    mahony_init(&c->ahrs, FUSION_KP, FUSION_KI);
    memcpy(c->pending_open, c->cfg.flex_open_mv, sizeof(c->pending_open));
    memcpy(c->pending_closed, c->cfg.flex_closed_mv, sizeof(c->pending_closed));

    c->imu_ok = app_hw_ok(HW_IMU);
    c->flex_ok = app_hw_ok(HW_FLEX);
    c->stat_t0_us = esp_timer_get_time();

    const BaseType_t ok = xTaskCreatePinnedToCore(sensor_task, "sensor", TASK_STACK_SENSOR, c, TASK_PRIO_SENSOR,
                                                  NULL, TASK_CORE_SENSOR);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "sensor loop %d Hz (imu %s, flex %s)", IMU_SAMPLE_HZ, c->imu_ok ? "on" : "missing",
             c->flex_ok ? "on" : "missing");
    return ESP_OK;
}
