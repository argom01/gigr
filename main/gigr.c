#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "mpu6050.h"
#include "lcd_st7796.h"
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

#define MPU6050_I2C_ADDR 0x68

static const char *TAG = "APP_MAIN";

void app_main(void) {
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

    ESP_LOGI(TAG, "Initializing ST7796 LCD...");
    esp_lcd_panel_handle_t lcd_panel = st7796_init(SPI2_HOST);

    uint16_t red = 0xF800;
    uint16_t green = 0x07E0;
    uint16_t black = 0x0000;
    uint16_t white = 0xFFFF;
    uint16_t blue = 0x001F;

    uint16_t *img = malloc(LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));
    for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) {
        if (i < LCD_WIDTH * LCD_HEIGHT / 4 && i % LCD_WIDTH < LCD_WIDTH / 2){
            ((uint16_t *)img)[i] = (blue << 8) | (blue >> 8);
        } else if (i < LCD_WIDTH * LCD_HEIGHT / 4) {
            ((uint16_t *)img)[i] = white;
        } else if (i < 2 * LCD_WIDTH * LCD_HEIGHT / 4) {
            ((uint16_t *)img)[i] = (red << 8) | (red >> 8);
        } else if (i < 3 * LCD_WIDTH * LCD_HEIGHT / 4) {
            ((uint16_t *)img)[i] = (green << 8) | (green >> 8);
        } else {
            ((uint16_t *)img)[i] = black;
        }
    }

    st7796_show_image(lcd_panel, img);
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "Displaying boot images...");
    for (int i = 0; i < SAMPLE_IMAGE_COUNT; i++) {
        st7796_show_image(lcd_panel, sample_images[i]);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    ESP_LOGI(TAG, "Initializing I2C Master Bus...");
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = SCL_PIN,
        .sda_io_num = SDA_PIN,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    i2c_master_dev_handle_t mpu_handle;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &mpu_handle));
    ESP_ERROR_CHECK(mpu6050_init(mpu_handle));

    mpu6050_data_t sensor_data;
    while (1) {
        if (mpu6050_read_data(mpu_handle, &sensor_data) == ESP_OK) {
            ESP_LOGI(TAG, "Accel [X:%6d Y:%6d Z:%6d] | Gyro [X:%6d Y:%6d Z:%6d]",
                     sensor_data.accel_x, sensor_data.accel_y, sensor_data.accel_z,
                     sensor_data.gyro_x, sensor_data.gyro_y, sensor_data.gyro_z);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}