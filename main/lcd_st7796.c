#include "lcd_st7796.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_commands.h"

#define LCD_SS_PIN      16
#define LCD_DC_PIN      17
#define LCD_RST_PIN     14
#define LCD_DIM_PIN     21

// Declare the st7796 init function provided by the registry component
extern esp_err_t esp_lcd_new_panel_st7796(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

esp_lcd_panel_handle_t st7796_init(spi_host_device_t spi_host) {
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC_PIN,
        .cs_gpio_num = LCD_SS_PIN,
        .pclk_hz = 40 * 1000 * 1000,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)spi_host, &io_config, &io_handle);

    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };

    esp_lcd_new_panel_st7796(io_handle, &panel_config, &panel_handle);

    esp_lcd_panel_reset(panel_handle);
    esp_lcd_panel_init(panel_handle);
    esp_lcd_panel_invert_color(panel_handle, false);
    esp_lcd_panel_swap_xy(panel_handle, false);
    esp_lcd_panel_mirror(panel_handle, true, false);
    esp_lcd_panel_disp_on_off(panel_handle, true);

    gpio_set_direction(LCD_DIM_PIN, GPIO_MODE_OUTPUT);
    st7796_set_backlight(1);

    return panel_handle;
}

void st7796_set_backlight(int level) {
    gpio_set_level(LCD_DIM_PIN, level);
}

void st7796_show_image(esp_lcd_panel_handle_t panel, const uint16_t *img) {
    const int chunk_lines = 40;

    for (int y = 0; y < LCD_HEIGHT; y += chunk_lines) {
        int lines_to_send = (y + chunk_lines <= LCD_HEIGHT) ? chunk_lines : (LCD_HEIGHT - y);

        esp_lcd_panel_draw_bitmap(panel,
                                  0,
                                  y,
                                  LCD_WIDTH,
                                  y + lines_to_send,
                                  &img[y * LCD_WIDTH]);
    }
}