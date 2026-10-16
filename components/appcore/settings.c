#include "settings.h"

#include <stdatomic.h>
#include <string.h>
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

static const char *TAG = "settings";

#define NVS_NAMESPACE   "gigr"
#define NVS_KEY         "settings"
#define SETTINGS_MAGIC  0x47494752u     // "GIGR"
#define SETTINGS_FORMAT 1u              // bump when settings_t changes layout
#define SAVE_DELAY_US   (1500 * 1000)

typedef struct {
    uint32_t magic;
    uint32_t format;
    uint32_t size;
    settings_t data;
} settings_blob_t;

static settings_t s_settings;
static SemaphoreHandle_t s_mutex;
static atomic_uint s_version;
static bool s_dirty;
static int64_t s_changed_us;

void settings_defaults(settings_t *s)
{
    memset(s, 0, sizeof(*s));

    s->backlight_pct = BACKLIGHT_DEFAULT_PCT;
    s->dim_timeout_s = BACKLIGHT_DIM_TIMEOUT_S;
    s->off_timeout_s = BACKLIGHT_OFF_TIMEOUT_S;
    s->perf_overlay = false;
    s->led_enabled = true;

    const float open_mv[FINGER_COUNT] = FLEX_DEFAULT_OPEN_MV;
    const float closed_mv[FINGER_COUNT] = FLEX_DEFAULT_CLOSED_MV;
    s->flex_cal_valid = false;
    memcpy(s->flex_open_mv, open_mv, sizeof(open_mv));
    memcpy(s->flex_closed_mv, closed_mv, sizeof(closed_mv));

    s->gyro_bias_valid = false;
    s->level_valid = false;
    s->level_quat[0] = 1.0f;

    s->am_enabled = true;
    s->am_x_source = AM_X_FROM_YAW;
    s->am_invert_x = false;
    s->am_invert_y = false;
    s->am_gain_min = AM_GAIN_MIN;
    s->am_gain_max = AM_GAIN_MAX;
    s->am_v1_dps = AM_V1_DPS;
    s->am_v2_dps = AM_V2_DPS;
    s->am_mincutoff_hz = AM_MINCUTOFF_HZ;
    s->am_beta = AM_BETA;
    s->am_deadzone_dps = AM_DEADZONE_DPS;
    s->am_press_th = AM_CLICK_PRESS_TH;
    s->am_release_th = AM_CLICK_RELEASE_TH;
    s->am_scroll_gain = AM_SCROLL_GAIN;

    s->gesture_enabled = true;
    s->gesture_th_offset = 0.0f;
    s->gesture_refractory_ms = GESTURE_REFRACTORY_MS;
    s->host_profile = 0;
    s->profile_per_peer = false;

    s->touchpad_gain = TOUCHPAD_GAIN;
}

static void load(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "no stored settings, using defaults");
        return;
    }
    settings_blob_t blob;
    size_t len = sizeof(blob);
    const esp_err_t err = nvs_get_blob(h, NVS_KEY, &blob, &len);
    nvs_close(h);

    if (err != ESP_OK || len != sizeof(blob) || blob.magic != SETTINGS_MAGIC ||
        blob.format != SETTINGS_FORMAT || blob.size != sizeof(settings_t)) {
        ESP_LOGW(TAG, "stored settings missing or from another format, using defaults");
        return;
    }
    s_settings = blob.data;
    ESP_LOGI(TAG, "settings loaded (flex cal %s, gyro bias %s, level %s)",
             s_settings.flex_cal_valid ? "yes" : "no",
             s_settings.gyro_bias_valid ? "yes" : "no",
             s_settings.level_valid ? "yes" : "no");
}

static void save(const settings_t *s)
{
    settings_blob_t blob = {
        .magic = SETTINGS_MAGIC,
        .format = SETTINGS_FORMAT,
        .size = sizeof(settings_t),
        .data = *s,
    };
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_blob(h, NVS_KEY, &blob, sizeof(blob));
        if (err == ESP_OK) {
            err = nvs_commit(h);
        }
        nvs_close(h);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "save failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "settings saved");
    }
}

void settings_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    settings_defaults(&s_settings);
    load();
    atomic_store(&s_version, 1);
}

void settings_get(settings_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_settings;
    xSemaphoreGive(s_mutex);
}

void settings_set(const settings_t *in)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_settings = *in;
    s_dirty = true;
    s_changed_us = esp_timer_get_time();
    xSemaphoreGive(s_mutex);
    atomic_fetch_add(&s_version, 1);
}

uint32_t settings_version(void)
{
    return atomic_load(&s_version);
}

void settings_service(void)
{
    settings_t copy;
    bool do_save = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_dirty && esp_timer_get_time() - s_changed_us > SAVE_DELAY_US) {
        copy = s_settings;
        s_dirty = false;
        do_save = true;
    }
    xSemaphoreGive(s_mutex);

    if (do_save) {
        save(&copy);
    }
}

void settings_save_now(void)
{
    settings_t copy;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    copy = s_settings;
    s_dirty = false;
    xSemaphoreGive(s_mutex);
    save(&copy);
}
