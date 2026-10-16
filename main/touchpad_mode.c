#include "touchpad_mode.h"

#include "app_state.h"
#include "board_config.h"
#include "esp_log.h"
#include "hid_tx.h"
#include "settings.h"
#include "touch.h"
#include "touchpad.h"

static const char *TAG = "touchpad";

static touchpad_t s_tp;
static uint32_t s_cfg_version;

static void on_touch(const touch_frame_t *f)
{
    if (settings_version() != s_cfg_version) {
        s_cfg_version = settings_version();
        settings_t cfg;
        settings_get(&cfg);
        s_tp.cfg.gain = cfg.touchpad_gain;
    }

    touchpad_out_t out;
    touchpad_process(&s_tp, f->n, f->x, f->y, f->t_us, &out);
    if (out.dx || out.dy || out.wheel || out.pan) {
        hid_tx_mouse_move(out.dx, out.dy, out.wheel, out.pan);
    }
    if (out.click) {
        hid_tx_mouse_click(out.click);
    }
    if (out.exit) {
        ESP_LOGI(TAG, "exit corner held");
        touchpad_reset(&s_tp);
        app_mode_set(APP_MODE_GESTURE);
    }
}

void touchpad_mode_init(void)
{
    const touchpad_cfg_t cfg = {
        .gain = TOUCHPAD_GAIN,
        .tap_max_ms = TOUCHPAD_TAP_MAX_MS,
        .tap_max_move_px = TOUCHPAD_TAP_MAX_MOVE_PX,
        .scroll_div = TOUCHPAD_SCROLL_DIV,
        .exit_corner_px = TOUCHPAD_EXIT_CORNER_PX,
        .exit_hold_ms = TOUCHPAD_EXIT_HOLD_MS,
    };
    touchpad_init(&s_tp, &cfg);
    touch_set_pad_handler(on_touch);
}
