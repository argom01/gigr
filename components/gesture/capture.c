#include "capture.h"

#include <stdatomic.h>
#include <stdio.h>
#include "app_state.h"
#include "esp_log.h"
#include "frame_ring.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "gesture_task.h"
#include "task_config.h"

static const char *TAG = "capture";

static TaskHandle_t s_task;
static atomic_bool s_running;
static uint32_t s_next;         // next frame index to print
static uint32_t s_samples;

static void on_frame(uint32_t idx)
{
    (void)idx;
    if (atomic_load(&s_running) && s_task) {
        xTaskNotifyGive(s_task);
    }
}

static void publish(void)
{
    const app_capture_state_t st = { .running = atomic_load(&s_running), .samples = s_samples };
    APP_STATE_PUT(capture, &st);
}

static void capture_task(void *arg)
{
    (void)arg;
    char line[160];
    float f[GESTURE_AXES];

    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200));
        if (!atomic_load(&s_running)) {
            continue;
        }
        if (app_mode_get() != APP_MODE_CAPTURE) {
            capture_stop();     // another mode was selected
            continue;
        }
        const uint32_t end = frame_ring_count();
        if (end - s_next > FRAME_RING_LEN / 2) {
            s_next = end;       // fell behind (console blocked): skip ahead
        }
        while (s_next < end) {
            if (!frame_ring_get(s_next, f)) {
                s_next = end;
                break;
            }
            int n = 0;
            for (int i = 0; i < GESTURE_AXES; i++) {
                n += snprintf(line + n, sizeof(line) - (size_t)n, i ? ",%.4f" : "%.4f", (double)f[i]);
            }
            line[n++] = '\n';
            fwrite(line, 1, (size_t)n, stdout);
            s_next++;
            s_samples++;
        }
        fflush(stdout);
        if (s_samples % 50 == 0) {
            publish();
        }
    }
}

esp_err_t capture_init(void)
{
    gesture_set_frame_listener(on_frame);
    const BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(capture_task, "capture", TASK_STACK_CAPTURE, NULL,
                                                          TASK_PRIO_CAPTURE, &s_task, TASK_CORE_CAPTURE,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void capture_start(void)
{
    if (atomic_load(&s_running)) {
        return;
    }
    ESP_LOGI(TAG, "streaming %d channels at 100 Hz, logs muted until stopped", GESTURE_AXES);
    esp_log_level_set("*", ESP_LOG_NONE);
    s_next = frame_ring_count();
    s_samples = 0;
    app_mode_set(APP_MODE_CAPTURE);
    atomic_store(&s_running, true);
    publish();
}

void capture_stop(void)
{
    if (!atomic_exchange(&s_running, false)) {
        return;
    }
    // only if nobody selected another mode meanwhile
    app_mode_transition(APP_MODE_CAPTURE, APP_MODE_GESTURE);
    esp_log_level_set("*", ESP_LOG_INFO);
    ESP_LOGI(TAG, "stopped after %lu samples", (unsigned long)s_samples);
    publish();
}

bool capture_running(void)
{
    return atomic_load(&s_running);
}
