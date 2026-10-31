#include "touchpad.h"

#include <math.h>
#include <string.h>

#define MB_LEFT   0x01
#define MB_RIGHT  0x02
#define MB_MIDDLE 0x04

void touchpad_init(touchpad_t *tp, const touchpad_cfg_t *cfg)
{
    memset(tp, 0, sizeof(*tp));
    tp->cfg = *cfg;
}

void touchpad_reset(touchpad_t *tp)
{
    const touchpad_cfg_t cfg = tp->cfg;
    touchpad_init(tp, &cfg);
}

static void centroid(int n, const uint16_t *x, const uint16_t *y, float *cx, float *cy)
{
    float sx = 0, sy = 0;
    for (int i = 0; i < n; i++) {
        sx += x[i];
        sy += y[i];
    }
    *cx = sx / (float)n;
    *cy = sy / (float)n;
}

static int32_t take_int(float *acc)
{
    const int32_t v = (int32_t)*acc;    // truncates towards zero, keeps the residual
    *acc -= (float)v;
    return v;
}

void touchpad_process(touchpad_t *tp, int n, const uint16_t *x, const uint16_t *y, int64_t t_us,
                      touchpad_out_t *out)
{
    memset(out, 0, sizeof(*out));

    if (n <= 0) {
        if (tp->active) {
            const int64_t dur_ms = (t_us - tp->t_start_us) / 1000;
            if (!tp->exit_sent && dur_ms <= tp->cfg.tap_max_ms && tp->travel <= tp->cfg.tap_max_move_px) {
                out->click = tp->max_fingers >= 3 ? MB_MIDDLE : (tp->max_fingers == 2 ? MB_RIGHT : MB_LEFT);
            }
        }
        touchpad_reset(tp);
        return;
    }

    float cx, cy;
    centroid(n, x, y, &cx, &cy);

    if (!tp->active) {
        tp->active = true;
        tp->t_start_us = t_us;
        tp->start_x = cx;
        tp->start_y = cy;
        tp->last_x = cx;
        tp->last_y = cy;
        tp->fingers = n;
        tp->max_fingers = n;
        return;
    }
    if (n > tp->max_fingers) {
        tp->max_fingers = n;
    }
    if (n != tp->fingers) {
        // finger added or lifted: re-anchor so the centroid jump is not motion
        tp->fingers = n;
        tp->last_x = cx;
        tp->last_y = cy;
        return;
    }

    const float dx = cx - tp->last_x;
    const float dy = cy - tp->last_y;
    tp->last_x = cx;
    tp->last_y = cy;
    tp->travel += fabsf(dx) + fabsf(dy);

    // exit corner: one finger resting in the top-right square (the GUI tab bar is on the left)
    if (n == 1 && tp->start_x >= tp->cfg.width - tp->cfg.exit_corner_px && tp->start_y < tp->cfg.exit_corner_px &&
        tp->travel <= tp->cfg.tap_max_move_px) {
        if (!tp->exit_sent && (t_us - tp->t_start_us) / 1000 >= tp->cfg.exit_hold_ms) {
            tp->exit_sent = true;
            out->exit = true;
        }
        return;
    }
    if (tp->exit_sent) {
        return;
    }

    if (n == 1) {
        tp->res_x += dx * tp->cfg.gain;
        tp->res_y += dy * tp->cfg.gain;
        out->dx = take_int(&tp->res_x);
        out->dy = take_int(&tp->res_y);
    } else if (n == 2) {
        // content follows the fingers: down -> wheel up (+), right -> pan left (-)
        tp->res_wheel += dy / tp->cfg.scroll_div;
        tp->res_pan -= dx / tp->cfg.scroll_div;
        out->wheel = take_int(&tp->res_wheel);
        out->pan = take_int(&tp->res_pan);
    }
}
