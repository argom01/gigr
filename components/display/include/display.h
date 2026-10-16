#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"

// ST7796 over SPI2 (IOMUX pins, DMA) + LVGL display in landscape (480x320).
// Only the GUI task may call LVGL after display_start(); before that the
// caller (app_main) may build the UI.
esp_err_t display_init(void);

// Starts the GUI task that runs lv_timer_handler()
esp_err_t display_start(void);

lv_display_t *display_get(void);

// true once the first complete frame has been sent (backlight may turn on)
bool display_first_frame_done(void);

#endif
