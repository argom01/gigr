#ifndef AIRMOUSE_H
#define AIRMOUSE_H

#include <stdbool.h>
#include <stdint.h>
#include "board.h"
#include "one_euro.h"

// Air mouse (pure C). Entered by curling ring + pinky; the hand's angular
// rate drives the cursor:
//   X: rotation about the vertical (yaw: wrist deviation / arm sweep), or
//      forearm roll if x_source = 1
//   Y: rotation about the horizontal axis across the hand (wrist flexion)
// Rates are taken about gravity-referenced axes, so the cursor moves the same
// way however the hand is rolled. A speed-dependent gain makes slow wrist
// motion precise and fast arm sweeps cover the whole screen.
// Index bend = left button, middle bend = right button, both held = scroll.

typedef struct {
    // pose
    float enter_th;
    float exit_th;
    int pose_ms;
    int settle_ms;
    // motion
    uint8_t x_source;           // 0 = yaw, 1 = forearm roll
    bool invert_x;
    bool invert_y;
    float gain_min;             // px/deg at low speed
    float gain_max;             // px/deg at high speed
    float v1_dps;               // gain starts rising
    float v2_dps;               // gain reaches gain_max
    float mincutoff_hz;
    float beta;
    float dcutoff_hz;
    float deadzone_dps;
    // clicks / scroll
    float press_th;
    float release_th;
    float dominance;
    int debounce_ms;
    int freeze_ms;
    float click_vel_th;         // bend units per second
    int scroll_hold_ms;
    float scroll_gain;          // wheel steps per degree
} airmouse_params_t;

typedef struct {
    bool active;
    bool entered;               // edge: became active this step
    bool exited;                // edge: left this step
    bool settling;
    bool frozen;
    bool scrolling;
    uint8_t buttons;            // bit0 left, bit1 right
    int32_t dx;
    int32_t dy;
    int32_t wheel;
    int32_t pan;
    float speed_dps;
    float gain;
} airmouse_out_t;

typedef struct {
    bool pressed;
    float cond_ms;              // how long the opposite-state condition has held
} am_button_t;

typedef struct {
    airmouse_params_t p;
    bool active;
    float pose_ms;              // time the enter (or exit) condition has held
    float settle_left_ms;
    float freeze_left_ms;
    float blend;                // 0..1 after a freeze
    one_euro_t fx;
    one_euro_t fy;
    float res_x;
    float res_y;
    float res_wheel;
    float res_pan;
    am_button_t left;
    am_button_t right;
    bool scrolling;
    float both_ms;
    float prev_index;
    float prev_middle;
    float vel_index;
    float vel_middle;
    bool prev_init;
} airmouse_t;

void airmouse_params_default(airmouse_params_t *p);
void airmouse_init(airmouse_t *am, const airmouse_params_t *p);
void airmouse_set_params(airmouse_t *am, const airmouse_params_t *p);
void airmouse_reset(airmouse_t *am);

// flex: normalised 0..1 per finger_t, gyro in dps (body frame, bias removed),
// up: unit world-up vector in the body frame, dt in seconds.
void airmouse_step(airmouse_t *am, bool enabled, const float flex[FINGER_COUNT], const float gyro_dps[3],
                   const float up[3], float dt, airmouse_out_t *out);

// Rates (deg/s) mapped to screen axes, before filtering. Exposed for tests.
void airmouse_screen_rates(const airmouse_params_t *p, const float gyro_dps[3], const float up[3], float *x_rate,
                           float *y_rate);

#endif
