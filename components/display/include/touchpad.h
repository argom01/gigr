#ifndef TOUCHPAD_H
#define TOUCHPAD_H

#include <stdbool.h>
#include <stdint.h>

// Pure C (no ESP-IDF): turns touch reports into relative mouse input.
//   1 finger drag      -> pointer motion
//   1 / 2 / 3 finger tap -> left / right / middle click
//   2 finger drag      -> vertical wheel + horizontal pan ("natural" direction)
//   hold the exit corner -> leave touchpad mode

typedef struct {
    float gain;             // host pixels per touch pixel
    int tap_max_ms;
    int tap_max_move_px;
    float scroll_div;       // touch pixels per wheel step
    int exit_corner_px;     // top-left square
    int exit_hold_ms;
} touchpad_cfg_t;

typedef struct {
    int32_t dx;
    int32_t dy;
    int32_t wheel;
    int32_t pan;
    uint8_t click;          // MB_* bit to click, 0 = none
    bool exit;
} touchpad_out_t;

typedef struct {
    touchpad_cfg_t cfg;
    bool active;
    int64_t t_start_us;
    int max_fingers;
    int fingers;
    float last_x;
    float last_y;
    float start_x;
    float start_y;
    float travel;
    float res_x;
    float res_y;
    float res_wheel;
    float res_pan;
    bool exit_sent;
} touchpad_t;

void touchpad_init(touchpad_t *tp, const touchpad_cfg_t *cfg);
void touchpad_reset(touchpad_t *tp);
void touchpad_process(touchpad_t *tp, int n, const uint16_t *x, const uint16_t *y, int64_t t_us,
                      touchpad_out_t *out);

#endif
