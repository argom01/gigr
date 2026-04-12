#include "gt911.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"

static const char *TAG = "GT911";

#define GT911_REG_CHIP_ID  0x8140
#define GT911_REG_STATUS   0x814E
#define GT911_REG_COORD    0x814F
#define I2C_TIMEOUT_MS     100

static esp_err_t gt911_read_reg(i2c_master_dev_handle_t handle, uint16_t reg, uint8_t *data, size_t len) {
    uint8_t reg_buf[2] = { reg >> 8, reg & 0xFF };
    return i2c_master_transmit_receive(handle, reg_buf, 2, data, len, I2C_TIMEOUT_MS);
}

static esp_err_t gt911_write_reg(i2c_master_dev_handle_t handle, uint16_t reg, uint8_t data) {
    uint8_t buf[3] = { reg >> 8, reg & 0xFF, data };
    return i2c_master_transmit(handle, buf, 3, I2C_TIMEOUT_MS);
}

i2c_master_dev_handle_t gt911_init(i2c_master_bus_handle_t bus_handle) {
    if (!bus_handle) return NULL;

    // --- Hardware Reset & Address Selection Sequence ---
    if (GT911_RST_PIN >= 0 && GT911_INT_PIN >= 0) {
        ESP_LOGI(TAG, "Performing hardware reset...");

        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << GT911_RST_PIN) | (1ULL << GT911_INT_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&io_conf);

        gpio_set_level(GT911_RST_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(10));

        // Set INT pin level to dictate the I2C address
        if (GT911_I2C_ADDR == 0x5D) {
            gpio_set_level(GT911_INT_PIN, 0);
        } else {
            gpio_set_level(GT911_INT_PIN, 1);
        }

        esp_rom_delay_us(500);
        gpio_set_level(GT911_RST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));

        // Reconfigure INT as input
        gpio_set_direction(GT911_INT_PIN, GPIO_MODE_INPUT);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // --- I2C Initialization ---
    i2c_master_dev_handle_t dev_handle = NULL;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = GT911_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add GT911 to I2C bus");
        return NULL;
    }

    uint8_t id[4] = {0};
    err = gt911_read_reg(dev_handle, GT911_REG_CHIP_ID, id, 3);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "GT911 initialized at 0x%02X. Chip ID: %c%c%c", GT911_I2C_ADDR, id[0], id[1], id[2]);
        return dev_handle;
    } else {
        ESP_LOGE(TAG, "Failed to read GT911 Chip ID at 0x%02X.", GT911_I2C_ADDR);
        i2c_master_bus_rm_device(dev_handle);
        return NULL;
    }
}

esp_err_t gt911_read_touches(i2c_master_dev_handle_t handle, gt911_point_t *points, uint8_t max_points, uint8_t *points_read) {
    if (!handle || !points || !points_read) return ESP_ERR_INVALID_ARG;

    *points_read = 0;
    uint8_t status = 0;

    esp_err_t err = gt911_read_reg(handle, GT911_REG_STATUS, &status, 1);
    if (err != ESP_OK) return err;

    if ((status & 0x80) == 0) return ESP_OK;

    uint8_t touch_count = status & 0x0F;
    if (touch_count > 5) touch_count = 5;

    if (touch_count > 0) {
        uint8_t to_read = (touch_count > max_points) ? max_points : touch_count;
        uint8_t buf[to_read * 8];
        err = gt911_read_reg(handle, GT911_REG_COORD, buf, to_read * 8);

        if (err == ESP_OK) {
            for (int i = 0; i < to_read; i++) {
                points[i].x = (buf[i*8 + 1] << 8) | buf[i*8 + 0];
                points[i].y = (buf[i*8 + 3] << 8) | buf[i*8 + 2];
                points[i].size = (buf[i*8 + 5] << 8) | buf[i*8 + 4];
            }
            *points_read = to_read;
        }
    }

    gt911_write_reg(handle, GT911_REG_STATUS, 0x00);
    return ESP_OK;
}