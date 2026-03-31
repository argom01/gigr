#pragma once

#include <stdint.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"

#define LCD_WIDTH  320
#define LCD_HEIGHT 480

esp_lcd_panel_handle_t st7796_init(spi_host_device_t spi_host);
void st7796_set_backlight(int level);
void st7796_show_image(esp_lcd_panel_handle_t panel, const uint16_t *img);