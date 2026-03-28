#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "mpu6050.h"

// --- User GPIO Definitions ---
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

#define MPU6050_I2C_ADDR 0x68

static const char *TAG = "APP_MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Initializing I2C Master Bus...");

    // Configure the I2C bus with the pins you provided
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1, // Let the driver auto-assign an I2C port
        .scl_io_num = SCL_PIN,
        .sda_io_num = SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    ESP_LOGI(TAG, "Registering MPU6050 device...");
    // Configure the specific device on the bus
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = 400000, // 400kHz Fast Mode
    };

    i2c_master_dev_handle_t mpu_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &mpu_handle));

    ESP_LOGI(TAG, "Waking up MPU6050...");
    ESP_ERROR_CHECK(mpu6050_init(mpu_handle));
    ESP_LOGI(TAG, "MPU6050 initialized successfully.");

    mpu6050_data_t sensor_data;

    while (1) {
        if (mpu6050_read_data(mpu_handle, &sensor_data) == ESP_OK) {
            ESP_LOGI(TAG, "Accel [X:%6d Y:%6d Z:%6d] | Gyro [X:%6d Y:%6d Z:%6d]",
                     sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
                     sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z);
        } else {
            ESP_LOGE(TAG, "Failed to read data from MPU6050");
        }

        // Log at 10Hz to prevent spamming the serial monitor too hard
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}