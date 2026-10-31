#include "hid_tx.h"

#include <stdatomic.h>
#include <string.h>
#include "app_state.h"
#include "ble_hid.h"
#include "board_config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "task_config.h"

static const char *TAG = "hid_tx";

#define FLUSH_MS        10      // mouse report rate cap (~ connection interval)
#define FLUSH_MIN_US    ((FLUSH_MS - 1) * 1000)     // a tick-rounded 10 ms wait may end up to 1 ms early
#define RETRY_MS        2       // poll interval while a report waits for a congested link
#define MAX_AGE_MS      500     // a tap that waited longer is dropped instead of sent late
#define PUBLISH_MS      500
#define QUEUE_LEN       16

typedef enum {
    EVT_KEYS,
    EVT_CONSUMER,
    EVT_CLICK,
} evt_type_t;

typedef struct {
    evt_type_t type;
    uint8_t mods;               // EVT_CLICK: the button
    uint8_t n;
    uint8_t keys[6];
    uint16_t usage;
    int64_t t_us;               // when it was queued
} hid_evt_t;

// A tap is sent as a small state machine instead of press/sleep/release, so
// the task never sleeps on a hold or on a congested link and mouse motion keeps
// flowing meanwhile. Taps are sent one at a time, in queue order.
typedef enum {
    TAP_IDLE,
    TAP_PRESS,                  // press report pending (link was busy)
    TAP_HOLD,                   // pressed, waiting for release_us
    TAP_RELEASE,                // release report pending (link was busy)
} tap_phase_t;

typedef struct {
    tap_phase_t phase;
    hid_evt_t evt;
    int64_t release_us;
} tap_t;

static QueueHandle_t s_queue;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static int32_t s_acc[4];            // dx, dy, wheel, pan
static uint8_t s_buttons;
static atomic_uint s_dropped;       // incremented by every producer

// hid_tx task only
static tap_t s_tap;
static uint8_t s_click_buttons;     // button of the click tap being held
static uint8_t s_sent_buttons;
static int64_t s_flush_us;
static uint32_t s_sent;

static void enqueue(hid_evt_t *e)
{
    if (!s_queue || !hid_device_connected()) {
        return;
    }
    e->t_us = esp_timer_get_time();
    if (xQueueSend(s_queue, e, 0) != pdTRUE) {
        atomic_fetch_add(&s_dropped, 1);
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
    hid_evt_t e = { .type = EVT_CONSUMER, .usage = usage };
    enqueue(&e);
}

void hid_tx_mouse_click(uint8_t button)
{
    hid_evt_t e = { .type = EVT_CLICK, .mods = button };
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

// Sends the accumulated motion at most every FLUSH_MS, and a button change at
// once. While the link is congested nothing is taken, so no motion is lost.
static void flush_mouse(int64_t now)
{
    if (hid_device_congested()) {
        return;
    }
    int32_t v[4];
    taskENTER_CRITICAL(&s_lock);
    const uint8_t buttons = s_buttons | s_click_buttons;
    if (buttons == s_sent_buttons && now - s_flush_us < FLUSH_MIN_US) {
        taskEXIT_CRITICAL(&s_lock);
        return;
    }
    v[0] = take_clamped(&s_acc[0], 32767);
    v[1] = take_clamped(&s_acc[1], 32767);
    v[2] = take_clamped(&s_acc[2], 127);
    v[3] = take_clamped(&s_acc[3], 127);
    taskEXIT_CRITICAL(&s_lock);

    if (!v[0] && !v[1] && !v[2] && !v[3] && buttons == s_sent_buttons) {
        return;
    }
    hid_mouse_send(buttons, (int16_t)v[0], (int16_t)v[1], (int8_t)v[2], (int8_t)v[3]);
    s_sent_buttons = buttons;
    s_flush_us = now;
    s_sent++;
}

static bool mouse_buttons_sent(void)
{
    taskENTER_CRITICAL(&s_lock);
    const uint8_t buttons = s_buttons | s_click_buttons;
    taskEXIT_CRITICAL(&s_lock);
    return buttons == s_sent_buttons;
}

// Sends the press (down) or release report of a tap; false while the link is busy
static bool tap_report(const hid_evt_t *e, bool down, int64_t now)
{
    if (e->type == EVT_CLICK) {
        s_click_buttons = down ? e->mods : 0;
        flush_mouse(now);
        return mouse_buttons_sent();
    }
    if (hid_device_congested()) {
        return false;
    }
    if (e->type == EVT_KEYS) {
        if (down) {
            hid_keyboard_send(e->mods, e->keys, e->n);
        } else {
            hid_keyboard_release();
        }
    } else {
        hid_consumer_send(down ? e->usage : 0);
    }
    s_sent++;
    return true;
}

static void tap_step(int64_t now)
{
    switch (s_tap.phase) {
    case TAP_IDLE:
        break;
    case TAP_PRESS:
        if (now - s_tap.evt.t_us > MAX_AGE_MS * 1000) {
            // the link stayed busy too long: a late shortcut is worse than none
            if (s_tap.evt.type == EVT_CLICK) {
                s_click_buttons = 0;
            }
            s_tap.phase = TAP_IDLE;
            atomic_fetch_add(&s_dropped, 1);
        } else if (tap_report(&s_tap.evt, true, now)) {
            s_tap.phase = TAP_HOLD;
            s_tap.release_us = now + GESTURE_KEY_HOLD_MS * 1000;
        }
        break;
    case TAP_HOLD:
        if (now < s_tap.release_us) {
            break;
        }
        s_tap.phase = TAP_RELEASE;
        /* fall through */
    case TAP_RELEASE:
        if (tap_report(&s_tap.evt, false, now)) {
            s_tap.phase = TAP_IDLE;
        }
        break;
    }
}

// How long the task may sleep before the tap or the mouse needs it again
static TickType_t next_wait(int64_t now)
{
    int64_t wait_ms = FLUSH_MS;
    if (s_tap.phase == TAP_PRESS || s_tap.phase == TAP_RELEASE) {
        wait_ms = RETRY_MS;
    } else if (s_tap.phase == TAP_HOLD) {
        const int64_t left_ms = (s_tap.release_us - now + 999) / 1000;
        wait_ms = left_ms < wait_ms ? left_ms : wait_ms;
    }
    const TickType_t t = pdMS_TO_TICKS(wait_ms > 0 ? wait_ms : 0);
    return t > 0 ? t : 1;
}

static void disconnected_reset(void)
{
    // drop stale motion and taps so nothing jumps or fires on reconnect
    taskENTER_CRITICAL(&s_lock);
    memset(s_acc, 0, sizeof(s_acc));
    taskEXIT_CRITICAL(&s_lock);
    s_sent_buttons = 0;
    s_click_buttons = 0;
    s_tap.phase = TAP_IDLE;
    xQueueReset(s_queue);
}

static void publish_state(void)
{
    app_ble_state_t st = {
        .connected = hid_device_connected(),
        .advertising = hid_device_advertising(),
        .reports_sent = s_sent,
        .reports_dropped = atomic_load(&s_dropped),
    };
    hid_device_peer(st.peer);
    APP_STATE_PUT(ble, &st);
}

static void hid_tx_task(void *arg)
{
    (void)arg;
    bool was_connected = false;
    int64_t published_us = 0;

    for (;;) {
        const TickType_t wait = next_wait(esp_timer_get_time());
        if (s_tap.phase == TAP_IDLE) {
            // wakes at once for a new tap, else after `wait` for the mouse
            if (xQueueReceive(s_queue, &s_tap.evt, wait) == pdTRUE) {
                s_tap.phase = TAP_PRESS;    // tap_step() drops it if it is already too old
            }
        } else {
            vTaskDelay(wait);
        }

        const int64_t now = esp_timer_get_time();
        const bool connected = hid_device_connected();
        if (connected) {
            tap_step(now);
            flush_mouse(now);
        } else {
            disconnected_reset();
        }
        if (connected != was_connected || now - published_us >= PUBLISH_MS * 1000) {
            was_connected = connected;
            published_us = now;
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
