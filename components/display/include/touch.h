#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#define TOUCH_MAX_POINTS 5

// One report from the controller, already in landscape display coordinates
typedef struct {
    uint8_t n;
    uint16_t x[TOUCH_MAX_POINTS];
    uint16_t y[TOUCH_MAX_POINTS];
    int64_t t_us;
} touch_frame_t;

// Called from the touch task for every report while in touchpad mode
typedef void (*touch_pad_cb_t)(const touch_frame_t *frame);

// GT911 on the shared I2C bus + LVGL pointer input device. Call after
// display_init() and before display_start(). ESP_ERR_NOT_FOUND = no chip.
esp_err_t touch_init(void);

// Starts the interrupt-driven touch task
esp_err_t touch_start(void);

void touch_set_pad_handler(touch_pad_cb_t cb);

#endif
