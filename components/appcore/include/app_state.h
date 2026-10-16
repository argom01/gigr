#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "board.h"

// Shared snapshot of everything the GUI shows. Producers write their own
// section with APP_STATE_PUT(); the GUI copies the whole struct with
// app_state_get(). Copies are protected by a short spinlock.

#define APP_MAX_GESTURES 16

typedef enum {
    APP_MODE_GESTURE = 0,   // CNN gestures -> keyboard (default)
    APP_MODE_AIRMOUSE,      // entered by pose (ring + pinky curled), if enabled
    APP_MODE_TOUCHPAD,      // LCD acts as a remote touchpad
    APP_MODE_CAPTURE,       // CSV streaming for Edge Impulse
    APP_MODE_COUNT
} app_mode_t;

typedef enum {
    HW_DISPLAY = 0,
    HW_TOUCH,
    HW_IMU,
    HW_FLEX,
    HW_BLE,
    HW_MODEL,
    HW_COUNT
} hw_unit_t;

typedef enum {
    HW_UNKNOWN = 0,
    HW_OK,
    HW_MISSING,
} hw_status_t;

typedef struct {
    int64_t t_us;
    float accel_g[3];           // body frame, levelled
    float gyro_dps[3];          // body frame, levelled, bias removed
    float quat[4];              // w, x, y, z (body -> world)
    float up[3];                // unit "up" vector in the body frame
    float roll_deg;
    float pitch_deg;
    float yaw_deg;              // relative only (no magnetometer)
    float flex_mv[FINGER_COUNT];
    float flex_norm[FINGER_COUNT];
    float gyro_bias_dps[3];
    bool still;
    float rate_hz;              // measured sensor loop rate
    float jitter_ms;            // worst period deviation over the last second
    float loop_us;              // time spent in one sensor iteration
    uint32_t imu_errors;
} app_sensor_state_t;

typedef enum {
    FLEXCAL_IDLE = 0,
    FLEXCAL_SETTLING,
    FLEXCAL_CAPTURING,
    FLEXCAL_DONE,
    FLEXCAL_NOISY,              // hold still and repeat
} flexcal_status_t;

typedef struct {
    flexcal_status_t status;
    uint8_t step;               // app_req_t of the step being captured
    float mean_mv[FINGER_COUNT];
    float std_mv[FINGER_COUNT];
    uint8_t span_ok_mask;       // bit per finger: open/closed span large enough
    uint8_t polarity_ok_mask;   // bit per finger: span has the expected sign
} app_flexcal_state_t;

typedef struct {
    uint16_t mv;
    uint8_t pct;
    bool charging;
    bool valid;
} app_battery_state_t;

typedef struct {
    bool connected;
    bool advertising;
    uint8_t peer[6];
    uint32_t reports_sent;
    uint32_t reports_dropped;
} app_ble_state_t;

typedef struct {
    uint8_t n_classes;
    float scores[APP_MAX_GESTURES];
    int8_t last_id;             // -1 = none yet
    float last_prob;
    int64_t last_t_us;
    uint32_t infer_us;
    uint32_t fired;
    bool model_stub;
} app_gesture_state_t;

typedef struct {
    bool active;
    bool settling;
    bool frozen;
    bool scrolling;
    uint8_t buttons;            // bit0 left, bit1 right
    float speed_dps;
    float gain;
    int32_t test_x;             // virtual cursor for the on-device test pad
    int32_t test_y;
} app_airmouse_state_t;

typedef struct {
    uint8_t points;
    uint16_t x;
    uint16_t y;
    uint32_t irq_count;
} app_touch_state_t;

typedef struct {
    bool running;
    uint32_t samples;
} app_capture_state_t;

typedef struct {
    float cpu_load[2];          // percent per core
    uint32_t heap_int_free;
    uint32_t heap_int_min;
    uint32_t heap_psram_free;
    uint32_t uptime_s;
} app_sys_state_t;

typedef struct {
    app_mode_t mode;
    hw_status_t hw[HW_COUNT];
    app_sensor_state_t sensor;
    app_flexcal_state_t flexcal;
    app_battery_state_t battery;
    app_ble_state_t ble;
    app_gesture_state_t gesture;
    app_airmouse_state_t airmouse;
    app_touch_state_t touch;
    app_capture_state_t capture;
    app_sys_state_t sys;
} app_state_t;

void app_state_init(void);

// Copy the whole snapshot
void app_state_get(app_state_t *out);

// Copy one section in/out (use the macros below)
void app_state_write(size_t offset, const void *src, size_t len);
void app_state_read(size_t offset, void *dst, size_t len);

#define APP_STATE_PUT(member, src) \
    app_state_write(offsetof(app_state_t, member), (src), sizeof(((app_state_t *)0)->member))
#define APP_STATE_TAKE(member, dst) \
    app_state_read(offsetof(app_state_t, member), (dst), sizeof(((app_state_t *)0)->member))

// Hardware presence
void app_hw_set(hw_unit_t unit, hw_status_t status);
hw_status_t app_hw_get(hw_unit_t unit);
static inline bool app_hw_ok(hw_unit_t unit) { return app_hw_get(unit) == HW_OK; }

// Operating mode
app_mode_t app_mode_get(void);
void app_mode_set(app_mode_t mode);
const char *app_mode_name(app_mode_t mode);

// Activity timestamp (touch, gestures...) used for backlight auto-dim
void app_activity_kick(void);
int64_t app_activity_last_us(void);

#endif
