#include "gesture_task.h"

#include <stdatomic.h>
#include <string.h>
#include "app_state.h"
#include "board_config.h"
#include "ei_glue.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "frame_ring.h"
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "hid_tx.h"
#include "postproc.h"
#include "profiles.h"
#include "settings.h"
#include "task_config.h"

static const char *TAG = "gesture";

#define WINDOW_VALUES (GESTURE_WINDOW_SAMPLES * GESTURE_AXES)

_Static_assert(GESTURE_COUNT <= APP_MAX_GESTURES, "too many gestures for app_state");

static TaskHandle_t s_task;
static atomic_int s_test_fire = -1;
static gesture_frame_cb_t s_frame_cb;

EXT_RAM_BSS_ATTR static float s_window[WINDOW_VALUES];
static float s_model_scores[32];
static int8_t s_label_map[32];      // model output -> dictionary id
static int s_model_labels;
static bool s_model_ok;

static settings_t s_cfg;
static uint32_t s_cfg_version;
static postproc_t s_pp;
static app_gesture_state_t s_state;

void gesture_feed(const float frame[GESTURE_AXES])
{
    frame_ring_push(frame);
    const uint32_t n = frame_ring_count();
    if (s_frame_cb) {
        s_frame_cb(n - 1);
    }
    if (s_task && n % GESTURE_STRIDE_SAMPLES == 0) {
        xTaskNotifyGive(s_task);
    }
}

void gesture_set_frame_listener(gesture_frame_cb_t cb)
{
    s_frame_cb = cb;
}

void gesture_fire_test(int id)
{
    if (id < 0 || id >= GESTURE_COUNT) {
        return;
    }
    atomic_store(&s_test_fire, id);
    if (s_task) {
        xTaskNotifyGive(s_task);
    }
}

static int find_peer(const settings_t *cfg, const uint8_t bda[6])
{
    for (int i = 0; i < SETTINGS_MAX_PEERS; i++) {
        if (cfg->peers[i].used && memcmp(cfg->peers[i].bda, bda, 6) == 0) {
            return i;
        }
    }
    return -1;
}

static int effective_profile(const settings_t *cfg)
{
    if (cfg->profile_per_peer) {
        app_ble_state_t ble;
        APP_STATE_TAKE(ble, &ble);
        const int slot = find_peer(cfg, ble.peer);
        if (ble.connected && slot >= 0 && cfg->peers[slot].profile < g_profile_count) {
            return cfg->peers[slot].profile;
        }
    }
    return cfg->host_profile < g_profile_count ? cfg->host_profile : 0;
}

int gesture_active_profile(void)
{
    settings_t cfg;
    settings_get(&cfg);
    return effective_profile(&cfg);
}

void gesture_bind_profile_to_peer(void)
{
    app_ble_state_t ble;
    APP_STATE_TAKE(ble, &ble);
    if (!ble.connected) {
        ESP_LOGW(TAG, "bind: no host connected");
        return;
    }
    settings_t *cfg = settings_edit_begin();
    int slot = find_peer(cfg, ble.peer);
    if (slot < 0) {
        for (int i = 0; i < SETTINGS_MAX_PEERS && slot < 0; i++) {
            if (!cfg->peers[i].used) {
                slot = i;
            }
        }
    }
    if (slot < 0) {
        // table full: drop the oldest entry
        memmove(&cfg->peers[0], &cfg->peers[1], sizeof(cfg->peers[0]) * (SETTINGS_MAX_PEERS - 1));
        slot = SETTINGS_MAX_PEERS - 1;
    }
    memcpy(cfg->peers[slot].bda, ble.peer, 6);
    const uint8_t profile = cfg->host_profile;
    cfg->peers[slot].profile = profile;
    cfg->peers[slot].used = 1;
    cfg->profile_per_peer = true;
    settings_edit_end();
    ESP_LOGI(TAG, "profile '%s' bound to %02x:%02x:%02x:%02x:%02x:%02x", g_profiles[profile].name,
             ble.peer[0], ble.peer[1], ble.peer[2], ble.peer[3], ble.peer[4], ble.peer[5]);
}

static void dispatch(int id, float prob, bool test)
{
    const int profile = effective_profile(&s_cfg);
    const action_t *a = profile_action(profile, id);
    char desc[40];
    action_describe(a, desc, sizeof(desc));

    switch (a->type) {
    case ACT_KEY:
        hid_tx_key(a->mods, a->key);
        break;
    case ACT_CONSUMER:
        hid_tx_consumer(a->usage);
        break;
    case ACT_MOUSE:
        hid_tx_mouse_click((uint8_t)a->usage);
        break;
    default:
        break;
    }
    ESP_LOGI(TAG, "%s%s p=%.2f -> %s [%s]", test ? "test " : "", gesture_name(id), (double)prob, desc,
             g_profiles[profile].name);

    s_state.last_id = (int8_t)id;
    s_state.last_prob = prob;
    s_state.last_t_us = esp_timer_get_time();
    s_state.fired++;
    app_activity_kick();
}

static void match_labels(void)
{
    memset(s_label_map, -1, sizeof(s_label_map));
    s_model_labels = eim_label_count();
    if (s_model_labels > (int)(sizeof(s_label_map))) {
        s_model_labels = sizeof(s_label_map);
    }
    int matched = 0;
    for (int i = 0; i < s_model_labels; i++) {
        const int id = gesture_find(eim_label(i));
        s_label_map[i] = (int8_t)id;
        if (id >= 0) {
            matched++;
        } else {
            ESP_LOGW(TAG, "model label '%s' is not in gesture_defs.h (ignored)", eim_label(i));
        }
    }
    for (int id = 0; id < GESTURE_COUNT; id++) {
        bool found = false;
        for (int i = 0; i < s_model_labels; i++) {
            found |= s_label_map[i] == id;
        }
        if (!found && s_model_labels > 0) {
            ESP_LOGW(TAG, "gesture '%s' is not produced by the model", gesture_name(id));
        }
    }
    s_state.matched_labels = (uint8_t)matched;
}

static void classify(int64_t now, float dt_ms)
{
    const uint32_t end = frame_ring_count();
    if (!frame_ring_window(end, GESTURE_WINDOW_SAMPLES, s_window)) {
        return;     // not enough history yet
    }
    uint32_t us = 0;
    const int n = eim_classify(s_window, WINDOW_VALUES, s_model_scores, s_model_labels, &us);
    if (n < 0) {
        ESP_LOGW(TAG, "classifier error %d", n);
        return;
    }
    float dict[GESTURE_COUNT] = { 0 };
    for (int i = 0; i < n; i++) {
        if (s_label_map[i] >= 0) {
            dict[s_label_map[i]] = s_model_scores[i];
        }
    }
    s_state.infer_us = us;
    memcpy(s_state.scores, dict, sizeof(dict));

    const bool allowed = s_cfg.gesture_enabled && app_mode_get() == APP_MODE_GESTURE;
    if (!allowed) {
        postproc_reset(&s_pp);
        return;
    }
    const int id = postproc_update(&s_pp, dict, now, dt_ms);
    if (id >= 0) {
        dispatch(id, dict[id], false);
    }
}

static void gesture_task(void *arg)
{
    (void)arg;
    int64_t last_us = 0;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
        const int64_t now = esp_timer_get_time();

        if (settings_version() != s_cfg_version) {
            s_cfg_version = settings_version();
            settings_get(&s_cfg);
            s_pp.th_offset = s_cfg.gesture_th_offset;
            s_pp.refractory_ms = s_cfg.gesture_refractory_ms;
        }

        const int test = atomic_exchange(&s_test_fire, -1);
        if (test >= 0) {
            dispatch(test, 1.0f, true);
        }

        if (s_model_ok) {
            const float dt_ms = last_us ? (float)(now - last_us) / 1000.0f : 100.0f;
            classify(now, dt_ms);
            last_us = now;
        }

        s_state.profile = (uint8_t)effective_profile(&s_cfg);
        APP_STATE_PUT(gesture, &s_state);
    }
}

esp_err_t gesture_start(void)
{
    settings_get(&s_cfg);
    s_cfg_version = settings_version();
    postproc_init(&s_pp, s_cfg.gesture_th_offset, s_cfg.gesture_refractory_ms);

    memset(&s_state, 0, sizeof(s_state));
    s_state.last_id = -1;
    s_state.n_classes = GESTURE_COUNT;
    s_state.model_stub = !eim_is_real();

    if (eim_is_real()) {
        match_labels();
        const bool shape_ok = eim_window_samples() == GESTURE_WINDOW_SAMPLES && eim_axes() == GESTURE_AXES;
        if (!shape_ok) {
            ESP_LOGE(TAG, "model expects %d samples x %d axes @ %d Hz, firmware provides %d x %d @ %d Hz",
                     eim_window_samples(), eim_axes(), eim_frequency_hz(), GESTURE_WINDOW_SAMPLES, GESTURE_AXES,
                     GESTURE_SAMPLE_HZ);
        }
        s_state.model_mismatch = !shape_ok;
        s_model_ok = shape_ok && s_state.matched_labels > 0;
        ESP_LOGI(TAG, "model: %d labels, %d matched", s_model_labels, s_state.matched_labels);
    } else {
        ESP_LOGW(TAG, "no Edge Impulse model in components/ei_model: gesture recognition off");
    }
    app_hw_set(HW_MODEL, s_model_ok ? HW_OK : HW_MISSING);
    APP_STATE_PUT(gesture, &s_state);

    // stack in PSRAM: this task never touches flash
    const BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(gesture_task, "infer", TASK_STACK_INFER, NULL,
                                                          TASK_PRIO_INFER, &s_task, TASK_CORE_INFER,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
