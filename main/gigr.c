#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "mpu6050.h"
#include "lcd_st7796.h"
#include "gt911.h"
#include "sample_images.h"
#include "ble_hid.h"

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

static const char *TAG = "APP_MAIN";

static bool was_touching = false;
static uint16_t last_x = 0;
static uint16_t last_y = 0;
const float SENSITIVITY = 0.02f;

static inline int8_t clamp_to_int8(float value) {
    if (value > 127.0f) return 127;
    if (value < -127.0f) return -127;
    return (int8_t)value;
}

void touch_event(gt911_point_t *points, uint8_t points_read) {
    if (points_read == 1) {
        if (!was_touching) {
            was_touching = true;
            last_x = points[0].x;
            last_y = points[0].y;
        } else {
            int raw_dx = points[0].x - last_x;
            int raw_dy = points[0].y - last_y;

            if (raw_dx != 0 || raw_dy != 0) {
                float scaled_dx = (float)raw_dx * SENSITIVITY;
                float scaled_dy = (float)raw_dy * SENSITIVITY;

                // Call your BLE function
                hid_touchpad_send(clamp_to_int8(scaled_dx), clamp_to_int8(scaled_dy), 0);

                last_x = points[0].x;
                last_y = points[0].y;
            }
        }
    }
    else if (points_read == 0) {
        if (was_touching) {
            was_touching = false;
            hid_touchpad_send(0, 0, 0); // Stop movement
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

    // Initialize BLE HID

    ESP_LOGI(TAG, "Initializing BLE HID...");
    hid_device_init("ESP32S3-HID");

    gt911_start_interrupt_task(gt911_handle, touch_event);

    mpu6050_data_t sensor_data;

    while (1) {
        // if (mpu6050_read_data(mpu_handle, &sensor_data) == ESP_OK) {
        //     ESP_LOGI(TAG, "Accel [X:%6d Y:%6d Z:%6d] | Gyro [X:%6d Y:%6d Z:%6d]",
        //              sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
        //              sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z);
        // }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}