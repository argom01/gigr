# GIGR – gesture recognition glove

Firmware for a glove-mounted ESP32-S3 (N16R8) controller. The glove is a
Bluetooth LE HID device (keyboard, consumer keys, mouse). It has three ways to
drive a host:

* **Gestures.** A 1D CNN trained in Edge Impulse runs on the ESP32-S3 with ESP-NN
  kernels and turns dynamic and static hand gestures into key combinations.
  Each OS/device has its own mapping (host profile).
* **Air mouse.** Curl the ring and pinky fingers to enter it. Wrist and arm
  rotation moves the cursor; index/middle bends are left/right clicks.
* **Touchpad.** The LCD acts as a remote touchpad.

An LVGL GUI on the 480×320 LCD shows every sensor reading and handles all
calibration, tuning and testing.

Hardware: MPU6500 (I2C), ST7796U LCD (SPI2 IOMUX, DMA), GT911 touch (I2C,
interrupt), 5 Spectra flex sensors + battery divider on ADC1, charge-status
pin. Pins are in `components/board/include/board.h`. Every assumption that
still has to be checked on real hardware is a `#define` in
`components/board/include/board_config.h`; see [TESTING.md](TESTING.md).

### GUI tabs

| Tab | What it covers |
|---|---|
| Home | mode, BLE, battery, last gesture, quick toggles, hardware status |
| Sens | flex bars with calibration points, IMU values, live chart |
| IMU | bubble level, axis check, level / gyro calibration, loop statistics |
| Flex | calibration wizard and quick re-zero |
| Gest | live class scores, thresholds, host profile, test fire |
| Mouse | air-mouse enable, mapping, gain/filter/click tuning, test pad |
| Pad | start the remote touchpad, speed |
| Data | Edge Impulse CSV streaming |
| Sys | backlight and auto-dim, LED, HID test, CPU/heap/stacks, touch test, reboot/reset |

## Building

```sh
. ~/esp/v5.5.1/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

Host unit tests (pure C modules, no hardware):

```sh
cmake -S test/host -B build-host && cmake --build build-host && ctest --test-dir build-host
```

## Architecture

| Component | Contents |
|---|---|
| `main` | `app_main` init order, 200 Hz `sensor_task`, `sysmon`, touchpad glue |
| `board` | pins, finger mapping, `board_config.h` (hardware guesses + defaults), `task_config.h`, LED / backlight PWM / charge pin |
| `appcore` | shared `app_state` snapshot, modes, GUI→task requests, NVS settings |
| `sensors` | shared I2C bus, MPU6500 (libdriver + shim), ADC continuous (flex + battery), flex calibration and battery maths |
| `fusion` | quaternions, Mahony AHRS, gyro bias / stillness, 1€ filter (pure C) |
| `airmouse` | air-mouse logic (pure C) |
| `gesture` | gesture dictionary, frame ring, post-processing, host profiles, classifier task, data capture |
| `ei_model` | Edge Impulse library glue (stub until a model is exported) |
| `ble_hid` | Bluedroid HID profile and `hid_tx` (the single report sender) |
| `display` | ST7796 + LVGL port, GT911 touch, touchpad logic |
| `gui` | LVGL screens |

Modules marked pure C have no ESP-IDF includes and are covered by `test/host`.

### Tasks and cores

| Core | Task | Prio | Trigger |
|---|---|---|---|
| 0 | Bluetooth controller + Bluedroid | IDF | – |
| 0 | `hid_tx` | 15 | queue / 10 ms flush (never sleeps on a key hold or a busy link) |
| 0 | `touch` | 12 | GT911 INT → task notification |
| 0 | `gui` (LVGL) | 4 | `lv_timer_handler` loop |
| 0 | `sysmon` | 2 | 10 Hz |
| 1 | `sensor` | 20 | IMU data-ready interrupt, 200 Hz (RTOS tick as fallback) |
| 1 | `infer` | 10 | every 10th 100 Hz frame |
| 1 | `capture` | 8 | every frame while streaming |

Core 1 does only the deterministic sensing and the ML. The 200 Hz loop never
waits behind Bluedroid or LVGL rendering. The GUI is the only task that
touches LVGL. Other tasks publish into `app_state` (short spinlock copies),
and the GUI reads it at 20 Hz. GUI actions reach the tasks as one-shot
requests (`app_req`) or settings changes (`settings_edit_begin/end`).
Automatic mode changes (air-mouse pose, touchpad exit corner, end of
streaming) use `app_mode_transition(from, to)`, a compare-and-swap, so they
never overwrite a mode the user has just selected.

### Sensor pipeline (200 Hz)

0. The MPU6500 data-ready pulse (`IMU_INT_PIN`) wakes the loop and its ISR
   timestamps the sample, so the loop runs on the IMU's clock and a delayed
   I2C read (touch traffic on the shared bus) does not shift the timestamp.
1. IMU burst read → gyro bias tracking in the sensor frame (stillness gated)
   → `IMU_MOUNT_MATRIX` → levelling rotation → Mahony → orientation, *up*
   vector.
2. ADC DMA drained and averaged (2 kHz per channel) → mV → two-point flex
   normalisation (0 = straight, 1 = bent). The sign of the span absorbs the
   inverted thumb divider. Optional resistance-domain linearisation.
3. Air mouse step (when enabled, calibrated, and in gesture/air-mouse mode).
4. Pairs of samples are averaged into 100 Hz frames for the classifier and the
   data capture. Both use `gesture_frame_build()`, so training and inference
   see exactly the same scaling.

## Gesture recognition (Edge Impulse)

### Input

11 channels at 100 Hz, one window = 1 s (100 samples):

```
ax ay az   [g]    / 2
gx gy gz   [dps]  / 500
thumb index middle ring pinky   0..1 (calibrated)
```

### 1. Record data

1. Calibrate the flex sensors (Flex tab) every time you put the glove on.
2. Data tab → **Start streaming**. Logs are muted, and gestures and the air
   mouse are paused.
3. On the PC: `edge-impulse-data-forwarder`. It detects 11 axes at ~100 Hz.
   Name them `ax,ay,az,gx,gy,gz,thumb,index,middle,ring,pinky`.
4. Record each gesture in Studio with its label, in several sessions. Take
   the glove off and on between sessions, and include other people if
   possible. Record plenty of **`idle`** (about 30–40 % of the data): typing,
   reaching, walking, resting, waving, the air-mouse pose. This is what keeps
   false triggers low.

Labels must match `components/gesture/include/gesture_defs.h` exactly.
Initial set: `idle`, `swipe_left`, `swipe_right`, `swipe_up`, `swipe_down`,
`fist`, `v_sign`, `thumbs_up`.

### 2. Impulse

* Time series, window size **1000 ms**, window increase 100–200 ms, frequency
  100 Hz, zero-pad data.
* Processing block: **Raw data**, scale axes = 1 (the firmware already
  normalises).
* Learning block: Classification, Keras **expert mode**.

### 3. Network

A small 1D CNN, about 12 k parameters (~12 KB int8) and ~0.37 M MACs, which
should take a few ms on the S3 with ESP-NN:

```
Input (100, 11)
Conv1D 16, k=5, same, ReLU  -> MaxPool1D 2        (50, 16)
Conv1D 32, k=5, same, ReLU  -> MaxPool1D 2        (25, 32)
Conv1D 64, k=3, same, ReLU  -> MaxPool1D 2        (12, 64)
Dropout 0.25 -> GlobalAveragePooling1D            (64)
Dense 32, ReLU -> Dropout 0.3 -> Dense N, softmax
```

In expert mode, replace only the model definition in the generated code. Keep
Edge Impulse's own dataset, callback and `fit` boilerplate around it:

```python
from tensorflow.keras.layers import (Reshape, Conv1D, MaxPooling1D, Dropout,
                                     GlobalAveragePooling1D, Dense)

model = Sequential()
model.add(Reshape((int(input_length / 11), 11), input_shape=(input_length, )))
model.add(Conv1D(16, kernel_size=5, padding='same', activation='relu'))
model.add(MaxPooling1D(pool_size=2, padding='same'))
model.add(Conv1D(32, kernel_size=5, padding='same', activation='relu'))
model.add(MaxPooling1D(pool_size=2, padding='same'))
model.add(Conv1D(64, kernel_size=3, padding='same', activation='relu'))
model.add(MaxPooling1D(pool_size=2, padding='same'))
model.add(Dropout(0.25))
model.add(GlobalAveragePooling1D())
model.add(Dense(32, activation='relu'))
model.add(Dropout(0.3))
model.add(Dense(classes, name='y_pred', activation='softmax'))
```

Suggested training settings: 60–100 epochs, learning rate 0.001, batch size
32. Use the data augmentation options (noise, time shift) if available, and
int8 quantisation. If accuracy plateaus, try a 1.5 s window or a fourth conv
block before anything recurrent (LSTMs are poorly supported by ESP-NN).

### 4. Deploy

1. Studio → Deployment → **C++ library**, quantised (int8), EON compiler on.
2. Unzip into a temporary folder and copy **only** `edge-impulse-sdk/`,
   `model-parameters/` and `tflite-model/` into `components/ei_model/`.
   Do not overwrite `components/ei_model/CMakeLists.txt`.
3. `idf.py fullclean build`. The configure step prints
   `ei_model: using the Edge Impulse export`.
4. Gesture tab: the number of matched labels and the inference time appear.
   Mismatches (unknown labels, wrong window/axes) are logged at boot.

> The Edge Impulse branch of `components/ei_model/CMakeLists.txt` was written
> without an export to build against. If the first build with a real model
> fails, look at the source globs/excludes and include directories there
> first (see TESTING.md).

### Post-processing

The classifier runs every 100 ms on the last second of data.

* Dynamic gestures fire when p ≥ threshold in 2 of the last 3 windows. After
  that comes a refractory period, then a lockout until `idle` wins again, so
  a single swipe fires once.
* Static poses fire after being held for `hold_ms`, once per hold.

The thresholds are per gesture in `gesture_defs.h`, plus a global offset and
the refractory time in the Gesture tab.

### Adding or changing gestures

1. Record and label the new gesture, retrain, export (steps above).
2. Add one `GESTURE(...)` line to `components/gesture/include/gesture_defs.h`
   (at most 16 in total, exactly one `K_IDLE`). Order does not matter, because
   model outputs are matched by label.
3. Optionally give it actions in `components/gesture/profiles.c`
   (`[G_NEW] = KEY(MOD_LCTRL, KC_Z)`); missing entries do nothing. If you
   remove a gesture, delete its lines in `profiles.c` too.

### Host profiles

`components/gesture/profiles.c` holds one hardcoded table per host: Windows,
macOS, Linux (GNOME), Android, iOS/iPadOS and Media only. Pick the default in
the Gesture tab. With *Remember a profile per paired host*, the button
*Use default profile for this host* binds the current profile to the
connected host's address. The glove then switches automatically when it
connects to that host. Actions can be key combos (`KEY(mods, key)`), consumer
usages (`CONS(CC_...)`: media, volume, AC Home/Back) or mouse clicks.

## Air mouse

* Enter by curling ring + pinky (> 0.65 for 150 ms); leave by opening either
  (< 0.45). Enable it in the Home or Mouse tab. It needs a flex calibration.
* Cursor X comes from rotation about the vertical axis (wrist deviation or arm
  sweep), or from forearm roll as an option. Cursor Y comes from rotation
  about the horizontal axis across the hand (wrist flexion). Both use
  gravity-referenced axes, so a rolled hand still moves the cursor
  horizontally.
* Speed-dependent gain: slow rotation uses ~4 px/° (wrist precision), fast
  rotation up to ~25 px/° (arm sweeps).
* Filtering: IMU DLPF 41 Hz → bias tracking → 1€ filter → soft dead zone →
  sub-pixel accumulation → one report per BLE connection interval.
* Index = left, middle = right (hysteresis, debounce, dominance check against
  tendon coupling). A fast bend freezes the cursor for 150 ms so clicks don't
  move it. Both fingers bent together = scroll mode (pitch → wheel,
  yaw → horizontal pan).
* All parameters are sliders in the Mouse tab. A test pad on the device shows
  the virtual cursor without a host.

## Touchpad

Pad tab → *Start touchpad*:

* 1 finger moves the cursor; tap = left click.
* 2 fingers scroll; 2-finger tap = right click; 3-finger tap = middle click.
* Hold the top-right corner for 1 s to exit.
