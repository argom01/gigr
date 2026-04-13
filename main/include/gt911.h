#ifndef GT911_H
#define GT911_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

#define GT911_I2C_ADDR   0x14
#define GT911_RST_PIN    6
#define GT911_INT_PIN    7

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t size;
} gt911_point_t;

/**
 * @brief Initialize the GT911 Touch Controller using default settings
 * @param bus_handle Initialized I2C master bus handle
 * @return i2c_master_dev_handle_t The initialized device handle, or NULL on failure
 */
esp_err_t gt911_init(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *out_handle);

/**
 * @brief Read touch points from the GT911
 * @param handle The raw I2C device handle returned by gt911_init
 * @param points Array of gt911_point_t to store the data
 * @param max_points Maximum number of points your array can hold
 * @param points_read Pointer to store the number of points actually read
 * @return esp_err_t ESP_OK on success
 */
esp_err_t gt911_read_touches(i2c_master_dev_handle_t handle, gt911_point_t *points, uint8_t max_points, uint8_t *points_read);

#endif // GT911_H