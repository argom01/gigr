#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdbool.h>
#include <stdint.h>
#include "board.h"

// Persistent user settings (NVS). Readers keep a local copy and refresh it
// when settings_version() changes. Writers call settings_set(); the write to
// flash is debounced by settings_service().

#define SETTINGS_MAX_PEERS 4

typedef enum {
    AM_X_FROM_YAW = 0,          // radial/ulnar deviation + arm sweep
    AM_X_FROM_ROLL,             // forearm pronation/supination
} am_x_source_t;

typedef struct {
    uint8_t bda[6];
    uint8_t profile;
    uint8_t used;
} settings_peer_t;

typedef struct {
    // display
    uint8_t backlight_pct;
    uint16_t dim_timeout_s;
    uint16_t off_timeout_s;
    bool perf_overlay;
    bool led_enabled;

    // flex calibration (millivolts per finger_t)
    bool flex_cal_valid;
    float flex_open_mv[FINGER_COUNT];
    float flex_closed_mv[FINGER_COUNT];

    // IMU
    bool gyro_bias_valid;
    float gyro_bias_dps[3];
    bool level_valid;
    float level_quat[4];        // body correction rotation, w x y z

    // air mouse
    bool am_enabled;
    uint8_t am_x_source;        // am_x_source_t
    bool am_invert_x;
    bool am_invert_y;
    float am_gain_min;
    float am_gain_max;
    float am_v1_dps;
    float am_v2_dps;
    float am_mincutoff_hz;
    float am_beta;
    float am_deadzone_dps;
    float am_press_th;
    float am_release_th;
    float am_scroll_gain;

    // gestures
    bool gesture_enabled;
    float gesture_th_offset;    // added to every per-class threshold
    uint16_t gesture_refractory_ms;
    uint8_t host_profile;
    bool profile_per_peer;
    settings_peer_t peers[SETTINGS_MAX_PEERS];

    // touchpad
    float touchpad_gain;
} settings_t;

void settings_init(void);
void settings_defaults(settings_t *s);

void settings_get(settings_t *out);
void settings_set(const settings_t *in);
uint32_t settings_version(void);

// Call periodically; writes to NVS once changes have settled
void settings_service(void);
void settings_save_now(void);

#endif
