#include "hid_tx.h"

#include <string.h>
#include "app_state.h"
#include "ble_hid.h"
#include "board_config.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "task_config.h"

static const char *TAG = "hid_tx";

#define FLUSH_MS        10      // mouse report rate cap (~ connection interval)
#define QUEUE_LEN       16
#define CONGEST_WAIT_MS 100

typedef enum {
    EVT_KEYS,
    EVT_CONSUMER,
    EVT_CLICK,
} evt_type_t;

typedef struct {
    evt_type_t type;
    uint8_t mods;
    uint8_t n;
    uint8_t keys[6];
    uint16_t usage;
} hid_evt_t;

static QueueHandle_t s_queue;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static int32_t s_acc[4];            // dx, dy, wheel, pan
static uint8_t s_buttons;
static uint8_t s_sent_buttons;
static uint32_t s_sent;
static uint32_t s_dropped;

static void enqueue(const hid_evt_t *e)
{
    if (!s_queue || !hid_device_connected()) {
        return;
    }
    if (xQueueSend(s_queue, e, 0) != pdTRUE) {
        s_dropped++;
    }
}

void hid_tx_keys(uint8_t mods, const uint8_t *keys, uint8_t n)
{
    hid_evt_t e = { .type = EVT_KEYS, .mods = mods, .n = n > 6 ? 6 : n };
    if (keys && e.n) {
        memcpy(e.keys, keys, e.n);
    }
    enqueue(&e);
}

void hid_tx_key(uint8_t mods, uint8_t key)
{
    hid_tx_keys(mods, &key, key ? 1 : 0);
}

void hid_tx_consumer(uint16_t usage)
{
    const hid_evt_t e = { .type = EVT_CONSUMER, .usage = usage };
    enqueue(&e);
}

void hid_tx_mouse_click(uint8_t button)
{
    const hid_evt_t e = { .type = EVT_CLICK, .mods = button };
    enqueue(&e);
}

void hid_tx_mouse_move(int32_t dx, int32_t dy, int32_t wheel, int32_t pan)
{
    taskENTER_CRITICAL(&s_lock);
    s_acc[0] += dx;
    s_acc[1] += dy;
    s_acc[2] += wheel;
    s_acc[3] += pan;
    taskEXIT_CRITICAL(&s_lock);
}

void hid_tx_mouse_buttons(uint8_t buttons)
{
    taskENTER_CRITICAL(&s_lock);
    s_buttons = buttons;
    taskEXIT_CRITICAL(&s_lock);
}

static int32_t take_clamped(int32_t *acc, int32_t lim)
{
    int32_t v = *acc;
    if (v > lim) {
        v = lim;
    } else if (v < -lim) {
        v = -lim;
    }
    *acc -= v;
    return v;
}

static bool wait_uncongested(void)
{
    for (int t = 0; t < CONGEST_WAIT_MS && hid_device_congested(); t += 5) {
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    return !hid_device_congested();
}

static void flush_mouse(uint8_t extra_buttons)
{
    int32_t v[4];
    uint8_t buttons;
    taskENTER_CRITICAL(&s_lock);
    v[0] = take_clamped(&s_acc[0], 32767);
    v[1] = take_clamped(&s_acc[1], 32767);
    v[2] = take_clamped(&s_acc[2], 127);
    v[3] = take_clamped(&s_acc[3], 127);
    buttons = s_buttons | extra_buttons;
    taskEXIT_CRITICAL(&s_lock);

    if (!v[0] && !v[1] && !v[2] && !v[3] && buttons == s_sent_buttons) {
        return;
    }
    if (hid_device_congested()) {
        // give the motion back, try again next flush
        hid_tx_mouse_move(v[0], v[1], v[2], v[3]);
        return;
    }
    hid_mouse_send(buttons, (int16_t)v[0], (int16_t)v[1], (int8_t)v[2], (int8_t)v[3]);
    s_sent_buttons = buttons;
    s_sent++;
}

static void handle_event(const hid_evt_t *e)
{
    if (!wait_uncongested()) {
        s_dropped++;
        return;
    }
    switch (e->type) {
    case EVT_KEYS:
        hid_keyboard_send(e->mods, e->keys, e->n);
        vTaskDelay(pdMS_TO_TICKS(GESTURE_KEY_HOLD_MS));
        wait_uncongested();
        hid_keyboard_release();
        s_sent += 2;
        break;
    case EVT_CONSUMER:
        hid_consumer_send(e->usage);
        vTaskDelay(pdMS_TO_TICKS(GESTURE_KEY_HOLD_MS));
        wait_uncongested();
        hid_consumer_send(0);
        s_sent += 2;
        break;
    case EVT_CLICK:
        flush_mouse(e->mods);
        vTaskDelay(pdMS_TO_TICKS(GESTURE_KEY_HOLD_MS));
        flush_mouse(0);
        break;
    }
}

static void publish_state(void)
{
    app_ble_state_t st = {
        .connected = hid_device_connected(),
        .advertising = hid_device_advertising(),
        .reports_sent = s_sent,
        .reports_dropped = s_dropped,
    };
    hid_device_peer(st.peer);
    APP_STATE_PUT(ble, &st);
}

static void hid_tx_task(void *arg)
{
    (void)arg;
    hid_evt_t e;
    bool was_connected = false;
    uint32_t n = 0;

    for (;;) {
        if (xQueueReceive(s_queue, &e, pdMS_TO_TICKS(FLUSH_MS)) == pdTRUE) {
            handle_event(&e);
        }

        const bool connected = hid_device_connected();
        if (connected) {
            flush_mouse(0);
        } else {
            // drop stale motion so the cursor does not jump on reconnect
            taskENTER_CRITICAL(&s_lock);
            memset(s_acc, 0, sizeof(s_acc));
            taskEXIT_CRITICAL(&s_lock);
            s_sent_buttons = 0;
            xQueueReset(s_queue);
        }
        if (connected != was_connected || ++n % 50 == 0) {
            was_connected = connected;
            publish_state();
        }
    }
}

esp_err_t hid_tx_start(void)
{
    s_queue = xQueueCreate(QUEUE_LEN, sizeof(hid_evt_t));
    if (!s_queue) {
        return ESP_ERR_NO_MEM;
    }
    const BaseType_t ok = xTaskCreatePinnedToCore(hid_tx_task, "hid_tx", TASK_STACK_HID_TX, NULL, TASK_PRIO_HID_TX,
                                                  NULL, TASK_CORE_HID_TX);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "started");
    return ESP_OK;
}
