#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// Hardware guesses and default tunables in one place.
// Everything marked VERIFY ON HW is an assumption made before the hardware
// could be tested -- TESTING.md says how to check each one and what to change.

// ── Glove ────────────────────────────────────────────────────────────────────

#define GLOVE_DEVICE_NAME           "GIGR Glove"

// Body frame used by the whole firmware (after IMU_MOUNT_MATRIX):
//   +X  towards the fingertips (forearm axis)
//   +Y  towards the thumb side for a right hand, palm down
//   +Z  up, out of the back of the hand
// Hand flat and palm down at rest => accel reads about (0, 0, +1 g).

// ── LCD (ST7796U, SPI2 via IOMUX) ────────────────────────────────────────────

#define LCD_SPI_HZ                  (40 * 1000 * 1000)  // try 80 MHz once the display works
#define LCD_SWAP_XY                 1   // VERIFY ON HW: landscape needs swapped axes
#define LCD_MIRROR_X                1   // VERIFY ON HW
#define LCD_MIRROR_Y                1   // VERIFY ON HW
#define LCD_INVERT_COLOR            0   // VERIFY ON HW: white looks black -> flip
#define LCD_BGR                     1   // VERIFY ON HW: red and blue swapped -> flip
#define LCD_DRAW_BUF_LINES          32  // lines per partial LVGL buffer (x2 buffers, internal DMA RAM)

// Backlight (LEDC PWM on GPIO46 through a MOSFET)
#define BACKLIGHT_PWM_HZ            20000
#define BACKLIGHT_DEFAULT_PCT       80
#define BACKLIGHT_DIM_PCT           10
#define BACKLIGHT_DIM_TIMEOUT_S     30   // 0 = never
#define BACKLIGHT_OFF_TIMEOUT_S     120  // 0 = never

// ── Touch (GT911 on the shared I2C bus) ──────────────────────────────────────
// Raw GT911 coordinates are transformed into display coordinates in this order:
// swap -> mirror X -> mirror Y (mirroring is done in landscape coordinates).

#define TOUCH_I2C_ADDR              0x5D  // 0x5D: INT held low during reset (driver does this)
#define TOUCH_SWAP_XY               1     // VERIFY ON HW
#define TOUCH_MIRROR_X              1     // VERIFY ON HW
#define TOUCH_MIRROR_Y              0     // VERIFY ON HW
#define TOUCH_INT_ACTIVE_HIGH       0     // either edge works (one per report pulse)
#define TOUCH_RELEASE_TIMEOUT_MS    120   // no report for this long while pressed = released

// Touchpad mode
#define TOUCHPAD_GAIN               1.6f  // host pixels per touch pixel
#define TOUCHPAD_TAP_MAX_MS         180
#define TOUCHPAD_TAP_MAX_MOVE_PX    10
#define TOUCHPAD_SCROLL_DIV         12.0f // touch pixels per wheel step
#define TOUCHPAD_EXIT_CORNER_PX     60    // top-left square, long press exits
#define TOUCHPAD_EXIT_HOLD_MS       1000

// ── I2C ──────────────────────────────────────────────────────────────────────

#define I2C_BUS_HZ                  400000

// ── IMU (MPU6500) ────────────────────────────────────────────────────────────

#define IMU_I2C_ADDR                0x68  // VERIFY ON HW: 0x69 if AD0 is pulled high
#define IMU_SAMPLE_HZ               200
#define IMU_ACCEL_RANGE_G           4
#define IMU_GYRO_RANGE_DPS          1000

// Sample pacing: the data-ready pulse on IMU_INT_PIN wakes the sensor loop, so
// it runs on the IMU's own clock and every sample is read exactly once. After
// IMU_DRDY_FAIL_COUNT missing edges in a row the loop falls back to the RTOS
// tick (the IMU tab then shows "timer").
#define IMU_USE_DRDY                1     // VERIFY ON HW: IMU INT wired to IMU_INT_PIN
#define IMU_DRDY_TIMEOUT_MS         15    // 3 sample periods
#define IMU_DRDY_FAIL_COUNT         20

// Sensor -> body frame rotation (row-major). The IMU sits on the bottom side of
// the PCB, i.e. rotated 180 deg about the board Y axis: x -> -x, z -> -z.
// VERIFY ON HW on the IMU screen (axis check) and change the signs/rows if needed.
#define IMU_MOUNT_MATRIX            { { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 } }

// Stillness detector used for gyro bias tracking
#define IMU_STILL_GYRO_DPS          1.5f
#define IMU_STILL_ACCEL_G           0.05f
#define IMU_STILL_TIME_S            1.0f
#define IMU_BIAS_TRACK_TAU_S        8.0f

// Mahony filter gains
#define FUSION_KP                   1.0f
#define FUSION_KI                   0.01f

// ── Flex sensors (ADC1, continuous mode) ─────────────────────────────────────

#define FLEX_ADC_TOTAL_HZ           12000   // 6 channels -> 2 kHz each
#define FLEX_LPF_HZ                 15.0f
// Placeholder calibration used until the wizard has been run (millivolts)
#define FLEX_DEFAULT_OPEN_MV        { 1000, 2300, 2300, 2300, 2300 }  // VERIFY ON HW
#define FLEX_DEFAULT_CLOSED_MV      { 2000, 1300, 1300, 1300, 1300 }  // VERIFY ON HW
#define FLEX_CAL_SETTLE_MS          500
#define FLEX_CAL_CAPTURE_MS         1500
#define FLEX_CAL_MAX_STD_MV         15.0f   // noisier than this during a capture -> "hold still"
#define FLEX_CAL_MIN_SPAN_MV        150.0f  // smaller open/closed difference -> rejected

// Optional: normalise in the resistance domain instead of voltage. Needs the
// real fixed resistor values. 0 = normalise millivolts (default).
#define FLEX_LINEARIZE_RESISTANCE   0
#define FLEX_VDD_MV                 3300.0f
#define FLEX_FIXED_R_OHM            { 47000.0f, 47000.0f, 47000.0f, 47000.0f, 47000.0f }  // VERIFY ON HW

// ── Battery ──────────────────────────────────────────────────────────────────

#define BAT_DIVIDER_RATIO           1.46f  // VERIFY ON HW: V_bat / V_adc
#define BAT_CHG_ACTIVE_LOW          1     // VERIFY ON HW: TP4056-style open-drain CHRG
#define BAT_CHG_PULLUP              1
#define BAT_CHARGE_OFFSET_MV        120   // voltage rise while charging, subtracted before the SoC lookup

// ── Air mouse defaults (all adjustable in the GUI) ───────────────────────────

#define AM_ENTER_TH                 0.65f // ring & pinky bend to enter
#define AM_EXIT_TH                  0.40f // either below this to exit
#define AM_POSE_MS                  150
#define AM_SETTLE_MS                120
#define AM_GAIN_MIN                 4.0f  // px per degree at slow speed (wrist)
#define AM_GAIN_MAX                 25.0f // px per degree at fast speed (arm)
#define AM_V1_DPS                   8.0f
#define AM_V2_DPS                   120.0f
#define AM_MINCUTOFF_HZ             1.0f
#define AM_BETA                     0.02f
#define AM_DCUTOFF_HZ               1.0f
#define AM_DEADZONE_DPS             1.0f
#define AM_CLICK_PRESS_TH           0.55f
#define AM_CLICK_RELEASE_TH         0.35f
#define AM_CLICK_DOMINANCE          0.20f
#define AM_CLICK_DEBOUNCE_MS        30
#define AM_CLICK_FREEZE_MS          150
#define AM_CLICK_VEL_TH             3.0f  // bend per second that counts as a click onset
#define AM_SCROLL_HOLD_MS           300
#define AM_SCROLL_GAIN              0.15f // wheel steps per degree

// ── Gesture recognition ──────────────────────────────────────────────────────

#define GESTURE_WINDOW_SAMPLES      100   // 1 s at 100 Hz
#define GESTURE_SAMPLE_HZ           100
#define GESTURE_STRIDE_SAMPLES      10    // classify every 100 ms
#define GESTURE_REFRACTORY_MS       600
#define GESTURE_KEY_HOLD_MS         20

#endif
