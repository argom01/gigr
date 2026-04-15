#include "gt911.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_rom_sys.h"

static const char *TAG = "GT911";

#define GT911_REG_CHIP_ID      0x8140
#define GT911_REG_STATUS       0x814E
#define GT911_REG_COORD        0x814F
#define GT911_REG_CONFIG_DATA  0x8047
#define GT911_REG_CHECKSUM     0x80FF
#define GT911_REG_CONFIG_FRESH 0x8100
#define GT911_CONFIG_SIZE      186

#define I2C_TIMEOUT_MS     100

static SemaphoreHandle_t s_touch_sem  = NULL;
static gt911_touch_cb_t s_touch_cb = NULL;
static i2c_master_dev_handle_t s_dev_handle = NULL;

static esp_err_t gt911_read_reg(i2c_master_dev_handle_t handle, uint16_t reg, uint8_t *data, size_t len) {
    uint8_t reg_buf[2] = { reg >> 8, reg & 0xFF };
    return i2c_master_transmit_receive(handle, reg_buf, 2, data, len, I2C_TIMEOUT_MS);
}

static esp_err_t gt911_write_reg(i2c_master_dev_handle_t handle, uint16_t reg, uint8_t data) {
    uint8_t buf[3] = { reg >> 8, reg & 0xFF, data };
    return i2c_master_transmit(handle, buf, 3, I2C_TIMEOUT_MS);
}

esp_err_t gt911_read_config(i2c_master_dev_handle_t handle, uint8_t *config_data) {
    if (!handle || !config_data) return ESP_ERR_INVALID_ARG;
    return gt911_read_reg(handle, GT911_REG_CONFIG_DATA, config_data, GT911_CONFIG_SIZE);
}

esp_err_t gt911_write_config(i2c_master_dev_handle_t handle, uint8_t *config_data) {
    if (!handle || !config_data) return ESP_ERR_INVALID_ARG;

    uint8_t checksum = 0;
    for (int i = 0; i < GT911_CONFIG_SIZE - 2; i++) {
        checksum += config_data[i];
    }
    checksum = (~checksum) + 1;

    ESP_LOGI(TAG, "checksum: %d", checksum);

    uint8_t write_buf[2 + GT911_CONFIG_SIZE];
    write_buf[0] = GT911_REG_CONFIG_DATA >> 8;
    write_buf[1] = GT911_REG_CONFIG_DATA & 0xFF;
    for(int i = 0; i < GT911_CONFIG_SIZE - 2; i++) {
        write_buf[2 + i] = config_data[i];
    }

    esp_err_t err = i2c_master_transmit(handle, write_buf, sizeof(write_buf), 200);
    if (err != ESP_OK) return err;

    uint8_t chk_buf[3] = {
        GT911_REG_CHECKSUM >> 8,
        GT911_REG_CHECKSUM & 0xFF,
        checksum
    };
    return i2c_master_transmit(handle, chk_buf, 3, 100);
}

esp_err_t gt911_apply_config(i2c_master_dev_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;

    uint8_t fresh_buf[3] = {
        GT911_REG_CONFIG_FRESH >> 8,
        GT911_REG_CONFIG_FRESH & 0xFF,
        0x01
    };

    esp_err_t err = i2c_master_transmit(handle, fresh_buf, 3, 100);

    vTaskDelay(pdMS_TO_TICKS(150));

    return err;
}

static void IRAM_ATTR gt911_isr_handler(void *arg) {
    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(s_touch_sem, &high_task_wakeup);
    if (high_task_wakeup) {
        portYIELD_FROM_ISR();
    }
}

static void gt911_touch_task(void *pvParameters) {
    gt911_point_t points[5];
    uint8_t points_read = 0;

    bool is_touching = false;

    while (1) {
        if (xSemaphoreTake(s_touch_sem, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (gt911_read_touches(s_dev_handle, points, 5, &points_read) == ESP_OK) {
                if (points_read > 0) {
                    is_touching = true;
                    if (s_touch_cb) {
                        s_touch_cb(points, points_read);
                    }
                } else if (is_touching) {
                    is_touching = false;
                    if (s_touch_cb) {
                        s_touch_cb(NULL, 0);
                    }
                }
            }
        } else {
            uint8_t status = 0;
            esp_err_t err = gt911_read_reg(s_dev_handle, GT911_REG_STATUS, &status, 1);

            if (err == ESP_OK) {
                uint8_t actual_fingers = status & 0x0F;
                if (is_touching && actual_fingers == 0) {
                    is_touching = false;
                    if (s_touch_cb) s_touch_cb(NULL, 0);
                }

                if (status != 0x00) {
                    gt911_write_reg(s_dev_handle, GT911_REG_STATUS, 0x00);
                }
            }
        }
    }
}

esp_err_t gt911_start_interrupt_task(i2c_master_dev_handle_t handle, gt911_touch_cb_t callback) {
    if (!handle || !callback) return ESP_ERR_INVALID_ARG;

    s_dev_handle = handle;
    s_touch_cb = callback;

    s_touch_sem = xSemaphoreCreateBinary();
    if (!s_touch_sem) {
        ESP_LOGE(TAG, "Failed to create touch semaphore");
        return ESP_ERR_NO_MEM;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << GT911_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    xTaskCreatePinnedToCore(gt911_touch_task, "gt911_touch_task", 4096, NULL, 10, NULL, 1);

    gpio_isr_handler_add(GT911_INT_PIN, gt911_isr_handler, NULL);
    ESP_LOGI(TAG, "Hardware interrupt task started on pin %d", GT911_INT_PIN);

    return ESP_OK;
}

esp_err_t gt911_init(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *out_handle) {
    if (!bus_handle) return ESP_ERR_INVALID_ARG;

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
        if (GT911_I2C_ADDR == 0x14) {
            gpio_set_level(GT911_INT_PIN, 1);
        } else {
            gpio_set_level(GT911_INT_PIN, 0);
        }

        esp_rom_delay_us(500);
        gpio_set_level(GT911_RST_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));

        // Reconfigure INT as input
        gpio_set_direction(GT911_INT_PIN, GPIO_MODE_INPUT);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    // --- I2C Initialization ---
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = GT911_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_cfg, out_handle);
    if (err != ESP_OK) return err;

    // Verify communication by reading the Chip ID ("911")
    uint8_t id[6] = {0};
    err = gt911_read_reg(*out_handle, GT911_REG_CHIP_ID, id, 6);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "GT911 initialized successfully. Chip ID: %c%c%c", id[0], id[1], id[2]);
    } else {
        ESP_LOGE(TAG, "Failed to read GT911 Chip ID. Check wiring or I2C address.");
    }

    return err;
}

esp_err_t gt911_read_touches(i2c_master_dev_handle_t handle, gt911_point_t *points, uint8_t max_points, uint8_t *points_read) {
    if (!handle || !points || !points_read) return ESP_ERR_INVALID_ARG;

    *points_read = 0;
    uint8_t status = 0;
    esp_err_t err;

    for (int retry = 0; retry < 5; retry++) {
        err = gt911_read_reg(handle, GT911_REG_STATUS, &status, 1);
        if (err != ESP_OK) return err;

        if ((status & 0x80) != 0) {
            break;
        }

        esp_rom_delay_us(500);
    }

    if ((status & 0x80) == 0) {
        return ESP_OK;
    }

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