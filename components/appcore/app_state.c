#include "app_state.h"

#include <stdatomic.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "app_state";

static app_state_t s_state;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static _Atomic int s_mode = APP_MODE_GESTURE;
static int64_t s_activity_us;

static const char *const s_mode_names[APP_MODE_COUNT] = {
    [APP_MODE_GESTURE] = "Gesture",
    [APP_MODE_AIRMOUSE] = "Air mouse",
    [APP_MODE_TOUCHPAD] = "Touchpad",
    [APP_MODE_CAPTURE] = "Capture",
};

void app_state_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.gesture.last_id = -1;
    s_activity_us = esp_timer_get_time();
}

void app_state_get(app_state_t *out)
{
    taskENTER_CRITICAL(&s_lock);
    memcpy(out, &s_state, sizeof(*out));
    taskEXIT_CRITICAL(&s_lock);
    out->mode = (app_mode_t)atomic_load(&s_mode);
}

void app_state_write(size_t offset, const void *src, size_t len)
{
    taskENTER_CRITICAL(&s_lock);
    memcpy((uint8_t *)&s_state + offset, src, len);
    taskEXIT_CRITICAL(&s_lock);
}

void app_state_read(size_t offset, void *dst, size_t len)
{
    taskENTER_CRITICAL(&s_lock);
    memcpy(dst, (const uint8_t *)&s_state + offset, len);
    taskEXIT_CRITICAL(&s_lock);
}

void app_hw_set(hw_unit_t unit, hw_status_t status)
{
    if (unit >= HW_COUNT) {
        return;
    }
    taskENTER_CRITICAL(&s_lock);
    s_state.hw[unit] = status;
    taskEXIT_CRITICAL(&s_lock);
}

hw_status_t app_hw_get(hw_unit_t unit)
{
    if (unit >= HW_COUNT) {
        return HW_UNKNOWN;
    }
    taskENTER_CRITICAL(&s_lock);
    const hw_status_t st = s_state.hw[unit];
    taskEXIT_CRITICAL(&s_lock);
    return st;
}

app_mode_t app_mode_get(void)
{
    return (app_mode_t)atomic_load(&s_mode);
}

void app_mode_set(app_mode_t mode)
{
    if (mode >= APP_MODE_COUNT) {
        return;
    }
    const int prev = atomic_exchange(&s_mode, (int)mode);
    if (prev != (int)mode) {
        ESP_LOGI(TAG, "mode: %s -> %s", s_mode_names[prev], s_mode_names[mode]);
    }
}

const char *app_mode_name(app_mode_t mode)
{
    return mode < APP_MODE_COUNT ? s_mode_names[mode] : "?";
}

void app_activity_kick(void)
{
    const int64_t now = esp_timer_get_time();
    taskENTER_CRITICAL(&s_lock);
    s_activity_us = now;
    taskEXIT_CRITICAL(&s_lock);
}

int64_t app_activity_last_us(void)
{
    taskENTER_CRITICAL(&s_lock);
    const int64_t t = s_activity_us;
    taskEXIT_CRITICAL(&s_lock);
    return t;
}
