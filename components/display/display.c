#include "display.h"

#include <stdatomic.h>
#include "app_state.h"
#include "board.h"
#include "board_config.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "task_config.h"

static const char *TAG = "display";

#define LCD_HOST SPI2_HOST
#define DRAW_BUF_PX (LCD_H_RES * LCD_DRAW_BUF_LINES)

static esp_lcd_panel_io_handle_t s_io;
static esp_lcd_panel_handle_t s_panel;
static lv_display_t *s_disp;
static atomic_bool s_first_frame;

static void log_cb(lv_log_level_t level, const char *buf)
{
    if (level >= LV_LOG_LEVEL_ERROR) {
        ESP_LOGE("lvgl", "%s", buf);
    } else {
        ESP_LOGW("lvgl", "%s", buf);
    }
}

static uint32_t tick_cb(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool IRAM_ATTR on_color_trans_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata,
                                          void *user_ctx)
{
    (void)io;
    (void)edata;
    lv_display_flush_ready((lv_display_t *)user_ctx);
    return false;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    // LVGL renders RGB565 already byte-swapped for SPI (LV_COLOR_FORMAT_RGB565_SWAPPED)
    esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    if (lv_display_flush_is_last(disp)) {
        atomic_store(&s_first_frame, true);
    }
}

static void probe_panel_id(void)
{
    // RDDID (0x04) needs MISO; a blank answer is only a hint, the panel is write-only otherwise
    uint8_t id[4] = { 0 };
    if (esp_lcd_panel_io_rx_param(s_io, 0x04, id, sizeof(id)) == ESP_OK) {
        ESP_LOGI(TAG, "panel id: %02x %02x %02x %02x", id[0], id[1], id[2], id[3]);
    } else {
        ESP_LOGW(TAG, "panel id read failed (MISO not connected?)");
    }
}

esp_err_t display_init(void)
{
    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_log_register_print_cb(log_cb);

    // keep LVGL objects out of the scarce internal RAM
    void *pool = heap_caps_malloc(LVGL_PSRAM_POOL_KB * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (pool) {
        lv_mem_add_pool(pool, LVGL_PSRAM_POOL_KB * 1024);
    } else {
        ESP_LOGW(TAG, "no PSRAM pool for LVGL, using the internal %d kB only", LV_MEM_SIZE / 1024);
    }

    s_disp = lv_display_create(LCD_H_RES, LCD_V_RES);
    ESP_RETURN_ON_FALSE(s_disp, ESP_ERR_NO_MEM, TAG, "lv_display_create");

    const spi_bus_config_t bus = {
        .sclk_io_num = LCD_SCK_PIN,
        .mosi_io_num = LCD_MOSI_PIN,
        .miso_io_num = LCD_MISO_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DRAW_BUF_PX * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO), TAG, "spi bus");

    const esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = LCD_DC_PIN,
        .cs_gpio_num = LCD_CS_PIN,
        .pclk_hz = LCD_SPI_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx = s_disp,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_io), TAG,
                        "panel io");

    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = LCD_RST_PIN,
        .rgb_ele_order = LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7796(s_io, &panel_cfg, &s_panel), TAG, "st7796");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    probe_panel_id();
    esp_lcd_panel_invert_color(s_panel, LCD_INVERT_COLOR);
    // Rotate in the panel (MADCTL) so LVGL renders landscape with no software rotation
    esp_lcd_panel_swap_xy(s_panel, LCD_SWAP_XY);
    esp_lcd_panel_mirror(s_panel, LCD_MIRROR_X, LCD_MIRROR_Y);
    esp_lcd_panel_disp_on_off(s_panel, true);

    void *buf1 = heap_caps_malloc(DRAW_BUF_PX * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *buf2 = heap_caps_malloc(DRAW_BUF_PX * sizeof(uint16_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    ESP_RETURN_ON_FALSE(buf1 && buf2, ESP_ERR_NO_MEM, TAG, "draw buffers");

    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_buffers(s_disp, buf1, buf2, DRAW_BUF_PX * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, flush_cb);

    ESP_LOGI(TAG, "LCD %dx%d @ %d MHz, buffers 2 x %u B", LCD_H_RES, LCD_V_RES, LCD_SPI_HZ / 1000000,
             (unsigned)(DRAW_BUF_PX * sizeof(uint16_t)));
    return ESP_OK;
}

static void gui_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t wait_ms = lv_timer_handler();
        if (wait_ms < 2) {
            wait_ms = 2;
        } else if (wait_ms > 20) {
            wait_ms = 20;
        }
        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

esp_err_t display_start(void)
{
    const BaseType_t ok =
        xTaskCreatePinnedToCore(gui_task, "gui", TASK_STACK_GUI, NULL, TASK_PRIO_GUI, NULL, TASK_CORE_GUI);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

lv_display_t *display_get(void)
{
    return s_disp;
}

bool display_first_frame_done(void)
{
    return atomic_load(&s_first_frame);
}
