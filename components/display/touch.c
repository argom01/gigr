#include "touch.h"

#include "app_state.h"
#include "board.h"
#include "board_config.h"
#include "board_io.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"
#include "lvgl.h"
#include "task_config.h"

static const char *TAG = "touch";

static esp_lcd_touch_handle_t s_tp;
static TaskHandle_t s_task;
static touch_pad_cb_t s_pad_cb;
static uint32_t s_irq_count;

// LVGL reads this cache; it is never read over I2C from the GUI task
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static bool s_pressed;
static uint16_t s_x;
static uint16_t s_y;

static void IRAM_ATTR touch_isr(esp_lcd_touch_handle_t tp)
{
    (void)tp;
    BaseType_t woken = pdFALSE;
    s_irq_count++;
    if (s_task) {
        vTaskNotifyGiveFromISR(s_task, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

static void indev_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    taskENTER_CRITICAL(&s_lock);
    data->point.x = s_x;
    data->point.y = s_y;
    data->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    taskEXIT_CRITICAL(&s_lock);
}

static void transform(uint16_t rx, uint16_t ry, uint16_t *ox, uint16_t *oy)
{
    int x = rx, y = ry;
    int w = LCD_NATIVE_H_RES, h = LCD_NATIVE_V_RES;
    if (TOUCH_SWAP_XY) {
        const int t = x;
        x = y;
        y = t;
        w = LCD_NATIVE_V_RES;
        h = LCD_NATIVE_H_RES;
    }
    if (TOUCH_MIRROR_X) {
        x = w - 1 - x;
    }
    if (TOUCH_MIRROR_Y) {
        y = h - 1 - y;
    }
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x >= LCD_H_RES) x = LCD_H_RES - 1;
    if (y >= LCD_V_RES) y = LCD_V_RES - 1;
    *ox = (uint16_t)x;
    *oy = (uint16_t)y;
}

static void touch_task(void *arg)
{
    (void)arg;
    bool down = false;
    bool swallow = false;   // ignore the rest of this touch (screen wake, touchpad exit)
    bool was_pad_mode = false;
    bool release_pending = false;   // empty report(s) while pressed, not yet confirmed
    int64_t release_t_us = 0;       // time of the first empty report
    touch_frame_t frame = { 0 };

    for (;;) {
        TickType_t wait = portMAX_DELAY;
        if (release_pending) {
            const int64_t left_us = release_t_us + TOUCH_RELEASE_DEBOUNCE_MS * 1000LL - esp_timer_get_time();
            wait = left_us > 0 ? pdMS_TO_TICKS((left_us + 999) / 1000) : 0;
            if (wait == 0) {
                wait = 1;
            }
        } else if (down) {
            wait = pdMS_TO_TICKS(TOUCH_RELEASE_TIMEOUT_MS);
        }
        const bool irq = ulTaskNotifyTake(pdTRUE, wait) > 0;

        frame.n = 0;
        // on a timeout while pressed, poll once: a held finger keeps reporting,
        // so no data now means the release pulse was missed
        if (irq || down) {
            esp_lcd_touch_point_data_t pts[TOUCH_MAX_POINTS];
            uint8_t n = 0;
            if (esp_lcd_touch_read_data(s_tp) == ESP_OK &&
                esp_lcd_touch_get_data(s_tp, pts, &n, TOUCH_MAX_POINTS) == ESP_OK) {
                for (uint8_t i = 0; i < n; i++) {
                    transform(pts[i].x, pts[i].y, &frame.x[i], &frame.y[i]);
                }
                frame.n = n;
            }
        }
        frame.t_us = esp_timer_get_time();

        // Debounce the release. A held finger can drop out for a report or two
        // (charger / USB ground noise, "no new data" reads, I2C errors), which
        // would otherwise look like a short tap. Hold the last pressed state
        // until the touch has stayed empty for TOUCH_RELEASE_DEBOUNCE_MS.
        if (down && frame.n == 0) {
            if (!release_pending) {
                release_pending = true;
                release_t_us = frame.t_us;
                continue;
            }
            if (frame.t_us - release_t_us < TOUCH_RELEASE_DEBOUNCE_MS * 1000LL) {
                continue;
            }
            // confirmed: date it to the first empty report so tap timing is unchanged
            frame.t_us = release_t_us;
        }
        release_pending = false;

        const bool now_down = frame.n > 0;
        if (now_down && !down && board_backlight_get() == 0) {
            swallow = true;
        }
        if (now_down) {
            app_activity_kick();
        }

        const bool pad_mode = app_mode_get() == APP_MODE_TOUCHPAD;
        if (was_pad_mode && !pad_mode && now_down) {
            swallow = true;     // the finger that held the exit corner must not click the GUI
        }
        was_pad_mode = pad_mode;
        taskENTER_CRITICAL(&s_lock);
        s_pressed = now_down && !swallow && !pad_mode;
        if (now_down) {
            s_x = frame.x[0];
            s_y = frame.y[0];
        }
        taskEXIT_CRITICAL(&s_lock);

        if (pad_mode && s_pad_cb && !swallow) {
            s_pad_cb(&frame);
        }
        if (!now_down) {
            swallow = false;
        }
        down = now_down;

        const app_touch_state_t ts = { .points = frame.n, .x = frame.x[0], .y = frame.y[0], .irq_count = s_irq_count };
        APP_STATE_PUT(touch, &ts);
    }
}

esp_err_t touch_init(void)
{
    i2c_master_bus_handle_t bus = i2c_bus_get();
    ESP_RETURN_ON_FALSE(bus, ESP_ERR_INVALID_STATE, TAG, "i2c bus not initialised");

    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    io_cfg.dev_addr = TOUCH_I2C_ADDR;
    io_cfg.scl_speed_hz = I2C_BUS_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io), TAG, "panel io");

    static esp_lcd_touch_io_gt911_config_t gt_cfg = { .dev_addr = TOUCH_I2C_ADDR };
    const esp_lcd_touch_config_t cfg = {
        .x_max = LCD_NATIVE_H_RES,
        .y_max = LCD_NATIVE_V_RES,
        .rst_gpio_num = TP_RST_PIN,
        .int_gpio_num = TP_INT_PIN,
        .levels = {
            .reset = 0,
            .interrupt = TOUCH_INT_ACTIVE_HIGH,
        },
        // orientation is applied in transform(), in landscape coordinates
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
        .interrupt_callback = touch_isr,
        .driver_data = &gt_cfg,
    };
    const esp_err_t err = esp_lcd_touch_new_i2c_gt911(io, &cfg, &s_tp);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "GT911 not found (%s)", esp_err_to_name(err));
        s_tp = NULL;
        esp_lcd_panel_io_del(io);
        return ESP_ERR_NOT_FOUND;
    }

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_read_cb);

    ESP_LOGI(TAG, "GT911 ready");
    return ESP_OK;
}

esp_err_t touch_start(void)
{
    ESP_RETURN_ON_FALSE(s_tp, ESP_ERR_INVALID_STATE, TAG, "not initialised");
    const BaseType_t ok =
        xTaskCreatePinnedToCore(touch_task, "touch", TASK_STACK_TOUCH, NULL, TASK_PRIO_TOUCH, &s_task, TASK_CORE_TOUCH);
    if (ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    // a report may already be pending from before the task existed
    xTaskNotifyGive(s_task);
    return ESP_OK;
}

void touch_set_pad_handler(touch_pad_cb_t cb)
{
    s_pad_cb = cb;
}
