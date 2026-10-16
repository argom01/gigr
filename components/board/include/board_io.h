#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// LED, backlight and charge-status pin. Safe to call before anything else.
esp_err_t board_io_init(void);

void board_led_set(bool on);

// 0..100 %, takes effect immediately
void board_backlight_set(uint8_t pct);
uint8_t board_backlight_get(void);

// true while the charger reports charging
bool board_is_charging(void);

#endif
