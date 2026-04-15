#include <stdio.h>
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

#include "driver/i2c_master.h"
#include "lcd_st7796.h"
#include "ble_hid.h"

#include "mpu6050.h"
#include "gt911.h"
#include "sample_images.h"

#define GAUGE_INT_PIN   GPIO_NUM_5
#define IMU_INT_PIN     GPIO_NUM_46
#define TP_RST_PIN      GPIO_NUM_6
#define TP_INT_PIN      GPIO_NUM_7
#define LCD_TE_PIN      GPIO_NUM_15
#define LCD_SS_PIN      GPIO_NUM_16
#define LCD_DC_PIN      GPIO_NUM_17
#define LCD_DIM_PIN     GPIO_NUM_21
#define LCD_RST_PIN     GPIO_NUM_14
#define SCL_PIN         GPIO_NUM_9
#define SDA_PIN         GPIO_NUM_10
#define SCK_PIN         GPIO_NUM_11
#define MOSI_PIN        GPIO_NUM_12
#define MISO_PIN        GPIO_NUM_13
#define LED_PIN         GPIO_NUM_40

#define DRAG_THRESHOLD_PX 5
#define TAP_TIMEOUT_MS 250

#define BTN_LEFT   0x01
#define BTN_RIGHT  0x02
#define BTN_MIDDLE 0x04

static const char *TAG = "APP_MAIN";

static bool was_touching = false;
static bool is_dragging = false;
static uint16_t start_x = 0;
static uint16_t start_y = 0;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
static uint32_t touch_start_time = 0;
static uint8_t max_fingers_seen = 0;

const float SENSITIVITY = 0.02f;

static inline int8_t clamp_to_int8(float value) {
    if (value > 127.0f) return 127;
    if (value < -127.0f) return -127;
    return (int8_t)value;
}

void touch_event(gt911_point_t *points, uint8_t points_read) {
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    //ESP_LOGI(TAG, "Touch event: %d point(s)", points_read);

    if (points_read > 0) {
        if (points_read > max_fingers_seen) {
            max_fingers_seen = points_read;
        }

        if (!was_touching) {
            // ESP_LOGI(TAG, "Touch started with %d point(s)", points_read);
            was_touching = true;
            is_dragging = false;
            touch_start_time = now_ms;

            start_x = points[0].x;
            start_y = points[0].y;
            last_x = points[0].x;
            last_y = points[0].y;

        } else {
            if (!is_dragging) {
                int dx_from_start = abs(points[0].x - start_x);
                int dy_from_start = abs(points[0].y - start_y);

                if (dx_from_start > DRAG_THRESHOLD_PX || dy_from_start > DRAG_THRESHOLD_PX) {
                    is_dragging = true;

                    last_x = points[0].x;
                    last_y = points[0].y;
                }
            }

            if (is_dragging && points_read == 1) {
                int raw_dx = points[0].x - last_x;
                int raw_dy = points[0].y - last_y;

                if (raw_dx != 0 || raw_dy != 0) {
                    float scaled_dx = (float)raw_dx * SENSITIVITY;
                    float scaled_dy = (float)raw_dy * SENSITIVITY;

                    hid_touchpad_send(clamp_to_int8(scaled_dx), clamp_to_int8(scaled_dy), 0);

                    last_x = points[0].x;
                    last_y = points[0].y;
                }
            }
        }
    }
    else if (points_read == 0) {
        if (was_touching) {
            was_touching = false;

            if (!is_dragging && (now_ms - touch_start_time) < TAP_TIMEOUT_MS) {
                // ESP_LOGI(TAG, "Tap detected with %d point(s), time: %d ms", max_fingers_seen, now_ms - touch_start_time);

                uint8_t btn_mask = 0;
                // if (max_fingers_seen == 1) btn_mask = BTN_LEFT;
                // else if (max_fingers_seen == 2) btn_mask = BTN_RIGHT;
                // else if (max_fingers_seen >= 3) btn_mask = BTN_MIDDLE;

                hid_touchpad_send(0, 0, btn_mask);

                vTaskDelay(pdMS_TO_TICKS(20));
            }

            hid_touchpad_send(0, 0, 0);

            max_fingers_seen = 0;
            is_dragging = false;
        }
    }
}

void app_main(void) {
    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install GPIO ISR service");
    }

    // Initialize buses
    ESP_LOGI(TAG, "Initializing SPI Bus...");
    spi_bus_config_t spi_bus_cfg = {
        .sclk_io_num = SCK_PIN,
        .mosi_io_num = MOSI_PIN,
        .miso_io_num = MISO_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &spi_bus_cfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Initializing I2C Master Bus...");
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = SCL_PIN,
        .sda_io_num = SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t i2c_bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

    // Initialize devices

    ESP_LOGI(TAG, "Initializing ST7796 LCD...");
    esp_lcd_panel_handle_t lcd_panel = st7796_init(SPI2_HOST);

    st7796_show_image(lcd_panel, sample_images[0]);

    i2c_master_dev_handle_t mpu_handle;
    ESP_ERROR_CHECK(mpu6050_init(i2c_bus_handle, &mpu_handle));

    i2c_master_dev_handle_t gt911_handle;
    ESP_ERROR_CHECK(gt911_init(i2c_bus_handle, &gt911_handle));

    // if (gt911_handle) {

    //     uint8_t config[186];

    //     if (gt911_read_config(gt911_handle, config) == ESP_OK) {
    //         ESP_LOGI(TAG, "Initial config version: %d, checksum: %d, fresh: %d", config[0], config[185], config[186]);
    //         // config[0] = 0;
    //         // config[0] = (config[0] + 1) & 0xFF;

    //         // config[1] = 320 & 0xFF;
    //         // config[2] = (320 >> 8) & 0xFF;
    //         // config[3] = 480 & 0xFF;
    //         // config[4] = (480 >> 8) & 0xFF;

    //         // config[6] = (3U << 4) | (0b1101) | (1U << 7);

    //         // config[8] = (1U << 6) | 0x02;
    //         // config[11] = 15;
    //         // config[12] = 100;
    //         // config[13] = 10;

    //         // config[15] = 0;

    //         gt911_write_config(gt911_handle, config);

    //         gt911_apply_config(gt911_handle);

    //         ESP_LOGI(TAG, "Custom configuration applied successfully!");
    //     }
    // }

    // Initialize BLE HID

    ESP_LOGI(TAG, "Initializing BLE HID...");
    hid_device_init("ESP32S3-HID");

    gt911_start_interrupt_task(gt911_handle, touch_event);

    mpu6050_data_t sensor_data;
    uint8_t config[186];

    gt911_read_config(gt911_handle, config);
    ESP_LOGI(TAG, "Current config version: %d", config[0]);
    ESP_LOGI(TAG, "dejitter: %d, threshold touch: %d, threshold release: %d", config[8], config[12], config[13]);
    ESP_LOGI(TAG, "res_x: %d, res_y: %d", (config[2] << 8) | config[1], (config[4] << 8) | config[3]);
    ESP_LOGI(TAG, "noise: %x", config[11]);
    ESP_LOGI(TAG, "m_sw: %x", config[6]);
    ESP_LOGI(TAG, "refresh: %d", config[15]);

    while (1) {
        // if (mpu6050_read_data(mpu_handle, &sensor_data) == ESP_OK) {
        //     ESP_LOGI(TAG, "Accel [X:%6d Y:%6d Z:%6d] | Gyro [X:%6d Y:%6d Z:%6d]",
        //              sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
        //              sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z);
        // }


        vTaskDelay(pdMS_TO_TICKS(500));

    }
}