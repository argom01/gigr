# Hardware bring-up checklist

The firmware was written and built before the hardware existed. This list
goes through every subsystem in a sensible order. For each one it says where
to look, what you should see, and which `#define` to change if it's wrong.
Unless noted otherwise, the `#define`s are in
`components/board/include/board_config.h` (`board_config.h` below).

Missing devices don't stop the boot. They are logged and shown as `MISSING`
under *Home → Hardware*, so you can bring the board up one part at a time.

Flash and watch the log:

```sh
idf.py -p /dev/ttyACM0 flash monitor
```

On a full board the boot log shows `MPU6500 ready`, `GT911 ready`,
`flex_adc: 6 channels`, `LCD 480x320`, `HID init done` and `sensor loop 200 Hz`.

---

## 1. Display

| Check | Expected | If wrong |
|---|---|---|
| Backlight | off during boot, on once the GUI is drawn | stays off: the *Display* line in the log, `LCD_DIM_PIN` |
| Orientation | tab bar on the left, text upright, landscape | change `LCD_SWAP_XY`, `LCD_MIRROR_X`, `LCD_MIRROR_Y` (8 combinations; swap must stay 1 for landscape) |
| Colours | dark theme, blue accents | black/white swapped: `LCD_INVERT_COLOR`; red/blue swapped: `LCD_BGR` |
| Speed | smooth tab switching | try `LCD_SPI_HZ` 80 MHz; FPS overlay under *System → FPS / CPU overlay* |

The log prints the panel ID read over MISO. All zeros or all `ff` only means
MISO is not connected; the panel is driven write-only.

## 2. Touch

*System → Touch test* shows a full-screen crosshair.

| Check | Expected | If wrong |
|---|---|---|
| Detected | Home: Touch OK | `TOUCH_I2C_ADDR` 0x5D ↔ 0x14 |
| Interrupts | `irq` counter rises while touching | `TOUCH_INT_ACTIVE_HIGH` (either edge should work), `TP_INT_PIN` |
| Orientation | crosshair under the finger, `(0,0)` top-left | axes exchanged: `TOUCH_SWAP_XY`; mirrored: `TOUCH_MIRROR_X` / `TOUCH_MIRROR_Y` |
| Range | bottom-right corner reaches ~ (479, 319) | GT911 may be configured for another resolution: scale in `transform()` in `components/display/touch.c` |
| Hold | crosshair stays while the finger rests still | `TOUCH_RELEASE_TIMEOUT_MS` |

## 3. IMU

*IMU* tab and *Sensors* tab.

| Check | Expected | If wrong |
|---|---|---|
| Detected | Home: IMU OK | `IMU_I2C_ADDR` 0x68 ↔ 0x69 |
| Loop | rate ~200 Hz, jitter < 1 ms, I2C errors 0 | bus wiring / pull-ups, `I2C_BUS_HZ` |
| Pacing | "(drdy)" after the rate, missed samples stays 0 (or rises only rarely) | "(timer)": no data-ready edges on `IMU_INT_PIN`; check the wiring or set `IMU_USE_DRDY 0`. Missed samples rising steadily: the loop overruns 5 ms (check `loop`) |
| Axis check, palm down | "Up: +Z" | fix `IMU_MOUNT_MATRIX` |
| Fingertips down | "Up: -X" | row 1 of the matrix |
| Thumb side up (right hand) | "Up: +Y" | row 2 of the matrix |
| Gyro sign | turning the hand left (CCW seen from above) gives positive gyro Z | flip the sign of that matrix row; keep the determinant +1 (flip two signs, or flip one and swap two rows) |

`IMU_MOUNT_MATRIX` maps sensor axes to body axes: row *i* says which sensor
axis (and sign) is body axis *i*. The default assumes the chip is rotated 180°
about the board Y axis because it sits on the bottom side.

Then:

1. **Gyro cal**: glove resting still → "Done", bias of a few dps or less.
2. **Level**: hand flat, palm down, still → bubble centred, roll/pitch ≈ 0.
3. Rest the glove. *still* should appear after ~1 s and the bias should stay
   stable.

## 4. Flex sensors

*Sensors* tab, raw values.

| Check | Expected | If wrong |
|---|---|---|
| Mapping | bending a finger moves its own row | pins / `FINGER_ADC_CHANNELS` in `board.h` |
| Range | a few hundred mV of change from straight to bent | divider resistors; ADC attenuation is 12 dB (0–3.1 V) |
| Direction | thumb rises with bend, the other fingers fall | `FINGER_VOLTAGE_RISES_ON_BEND` in `board.h` (only affects the polarity warning) |
| Noise | a few mV at rest | `FLEX_LPF_HZ` |

Calibrate in the *Flex* tab: **1 Open hand → 2 Fist → 3 Thumb tucked → Save**.
Each finger should report *span ok* and no polarity warning. Afterwards the
*Sensors* bars go 0 → 1, the UNCALIBRATED banner disappears, and the air mouse
can be entered. *Quick re-zero* after putting the glove back on keeps the
spans.

Optional: set `FLEX_LINEARIZE_RESISTANCE 1` and the real `FLEX_FIXED_R_OHM` /
`FLEX_VDD_MV` to normalise in the resistance domain (more linear in bend
angle). Then recalibrate.

## 5. Battery and charging

| Check | Expected | If wrong |
|---|---|---|
| Voltage | Home shows the cell voltage within ~50 mV of a multimeter | `BAT_DIVIDER_RATIO` ← old ratio × V_meter / V_shown |
| Charging | ⚡ icon only while charging | `BAT_CHG_ACTIVE_LOW`; floating input: `BAT_CHG_PULLUP` |
| Percentage | moves slowly and does not jump up while discharging | `BAT_CHARGE_OFFSET_MV` for the jump when the charger is plugged in |

The host sees the same percentage through the BLE Battery Service.

## 6. Bluetooth HID

1. Pair **GIGR Glove** from a PC and from a phone ("just works", no PIN).
2. Use the *System → Bluetooth HID test* buttons:
   * *Type 'a'*: an "a" appears.
   * *Vol +*: the volume changes.
   * *Move mouse*: the cursor draws a small square.
   * *Scroll*: the page scrolls.
3. The log shows `conn params: interval 7.5–15 ms`. Some hosts choose their own
   interval.
4. Reboot the glove: it should reconnect without pairing again.
   *Forget hosts* clears the bonds.

## 7. Touchpad

*Pad* tab → *Start touchpad*:

* move, tap, 2-finger scroll in both directions, 2-finger tap (right click);
* hold the top-left corner for 1 s to exit;
* adjust the speed with the slider.

Scroll direction follows the fingers ("natural"). To reverse it, flip the signs
in `touchpad_process()` (`components/display/touchpad.c`).

## 8. Air mouse

It needs a flex calibration. *Mouse* tab:

1. Curl ring + pinky: the status shows *settling*, then *ACTIVE*. Open them:
   *inactive*. If it's hard to enter or leave, change `AM_ENTER_TH` /
   `AM_EXIT_TH`.
2. Test pad, with no host needed:
   * turning right (or rolling right with *Forearm roll*) moves the dot right;
   * fingertips down moves it down.
   If not, use *Invert X / Invert Y*.
3. Tuning order:
   1. raise the dead zone until the dot rests while the hand is still;
   2. set the slow gain for comfortable wrist precision;
   3. set the fast gain for a full-screen arm sweep;
   4. adjust the 1€ cut-off (jitter) and beta (lag).
4. Clicks:
   * an index bend gives a left click and a middle bend a right click; the dot
     must not jump while clicking (click freeze);
   * holding index while moving drags;
   * bending both fingers together for 0.3 s gives scroll mode.
   The tendon-coupling and click-velocity constants are `AM_CLICK_*` in
   `board_config.h`.
5. Repeat with a host connected.

## 9. Data capture and training

Follow the steps in README → *Gesture recognition*. Check that
`edge-impulse-data-forwarder` detects **11 axes at ~100 Hz**. Before
recording, move each finger and confirm the right column changes.

## 10. Real model

1. After copying the export into `components/ei_model/`, run
   `idf.py fullclean build`.
2. If the Edge Impulse SDK does not compile, check in
   `components/ei_model/CMakeLists.txt`:
   * the source globs (some SDK versions move ESP-NN or add files for other
     targets — exclude them with `list(FILTER ...)`);
   * the include directories;
   * the `EI_*` definitions.
   Edge Impulse's standalone ESP32 example project lists the exact set for
   your SDK version.
3. At boot, look for:
   * `model: N labels, N matched`;
   * no "not in gesture_defs.h" warnings;
   * no window/axes mismatch error.
4. Gesture tab:
   * the inference time should be a few ms;
   * idle should dominate while the hand is at rest;
   * each gesture's bar should rise when you perform it.
5. Tune each gesture's `min_prob` in `gesture_defs.h`, the global threshold
   offset and the refractory time.
6. Run a false-trigger test: 5 minutes of normal hand use with *fired* not
   increasing.

## 11. Host profiles

Pick the default profile for each host. Use *Fire* in the Gesture tab to check
each mapping without the model, then turn on *Remember a profile per paired
host* and bind a profile on each host. Shortcuts that depend on the OS
version or desktop settings (GNOME workspaces, Android recents) may need
different entries in `components/gesture/profiles.c`.

## 12. System health

The *System* tab shows:

* CPU load per core;
* internal/PSRAM/LVGL heap;
* task stack high-water marks (free words).

Expected: core 1 well under 50 %, internal heap free > 30 kB, every stack
with > 500 words free. Stack sizes and priorities are in
`components/board/include/task_config.h`.

## Not yet used

* `LCD_TE_PIN` (tearing sync) is wired but not used yet. `IMU_INT_PIN` is
  used for data-ready only, not for wake-on-motion.
* No power saving yet (light sleep, IMU low-power): the backlight auto-dim is
  the only power feature so far.
