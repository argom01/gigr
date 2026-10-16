#include "sysmon.h"

#include <string.h>
#include "app_state.h"
#include "ble_hid.h"
#include "board_config.h"
#include "board_io.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings.h"
#include "task_config.h"

static const char *TAG = "sysmon";

#define SYSMON_PERIOD_MS 100

static void update_cpu_load(app_sys_state_t *sys)
{
    static uint32_t prev_idle[2];
    static int64_t prev_wall;

    const int64_t wall = esp_timer_get_time();
    const int64_t dt = wall - prev_wall;
    for (int core = 0; core < 2; core++) {
        const uint32_t now = (uint32_t)ulTaskGetIdleRunTimeCounterForCore(core);
        if (prev_wall != 0 && dt > 0) {
            const float idle_pct = 100.0f * (float)(uint32_t)(now - prev_idle[core]) / (float)dt;
            float load = 100.0f - idle_pct;
            if (load < 0.0f) {
                load = 0.0f;
            }
            sys->cpu_load[core] = load;
        }
        prev_idle[core] = now;
    }
    prev_wall = wall;
}

static void update_led(const app_state_t *st, const settings_t *cfg, uint32_t tick)
{
    static uint32_t last_fired;
    static uint32_t flash_until;

    if (!cfg->led_enabled) {
        board_led_set(false);
        return;
    }
    if (st->gesture.fired != last_fired) {
        last_fired = st->gesture.fired;
        flash_until = tick + 2;
    }

    bool on;
    if (st->ble.connected) {
        // connected: solid, double blink pattern in air-mouse mode
        on = true;
        if (st->mode == APP_MODE_AIRMOUSE) {
            const uint32_t phase = tick % 10;
            on = !(phase == 0 || phase == 2);
        }
    } else {
        // advertising / idle: short blink once a second
        on = (tick % 10) == 0;
    }
    if (tick < flash_until) {
        on = !on;
    }
    board_led_set(on);
}

static void update_backlight(const app_state_t *st, const settings_t *cfg)
{
    if (st->hw[HW_DISPLAY] != HW_OK || !display_first_frame_done()) {
        return;
    }
    const int64_t idle_s = (esp_timer_get_time() - app_activity_last_us()) / 1000000;
    uint8_t pct = cfg->backlight_pct;
    if (st->mode != APP_MODE_TOUCHPAD) {
        if (cfg->off_timeout_s && idle_s >= cfg->off_timeout_s) {
            pct = 0;
        } else if (cfg->dim_timeout_s && idle_s >= cfg->dim_timeout_s && pct > BACKLIGHT_DIM_PCT) {
            pct = BACKLIGHT_DIM_PCT;
        }
    }
    if (pct != board_backlight_get()) {
        board_backlight_set(pct);
    }
}

static void update_ble_battery(const app_state_t *st)
{
    static int last_pct = -1;
    static bool last_connected;
    if (!st->battery.valid || st->hw[HW_BLE] != HW_OK) {
        return;
    }
    // notify on change, and once after every (re)connection
    if (st->battery.pct != last_pct || st->ble.connected != last_connected) {
        last_pct = st->battery.pct;
        last_connected = st->ble.connected;
        hid_device_set_battery(st->battery.pct);
    }
}

static void sysmon_task(void *arg)
{
    (void)arg;
    uint32_t tick = 0;
    settings_t cfg;
    uint32_t cfg_version = 0;
    app_state_t *st = heap_caps_malloc(sizeof(app_state_t), MALLOC_CAP_DEFAULT);
    assert(st);

    TickType_t wake = xTaskGetTickCount();
    for (;;) {
        if (settings_version() != cfg_version) {
            cfg_version = settings_version();
            settings_get(&cfg);
        }
        app_state_get(st);

        update_led(st, &cfg, tick);
        update_backlight(st, &cfg);
        update_ble_battery(st);

        if (tick % 10 == 0) {
            app_sys_state_t sys = st->sys;
            update_cpu_load(&sys);
            sys.heap_int_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
            sys.heap_int_min = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
            sys.heap_psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
            sys.uptime_s = (uint32_t)(esp_timer_get_time() / 1000000);
            APP_STATE_PUT(sys, &sys);
            settings_service();
        }

        tick++;
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(SYSMON_PERIOD_MS));
    }
}

esp_err_t sysmon_start(void)
{
    const BaseType_t ok = xTaskCreatePinnedToCore(sysmon_task, "sysmon", TASK_STACK_SYSMON, NULL,
                                                  TASK_PRIO_SYSMON, NULL, TASK_CORE_SYSMON);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "task create failed");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
