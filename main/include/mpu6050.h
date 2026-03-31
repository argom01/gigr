#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

typedef struct {
    int16_t accel_x;
    int16_t accel_y;
    int16_t accel_z;
    int16_t gyro_x;
    int16_t gyro_y;
    int16_t gyro_z;
    int16_t temp;
} mpu6050_data_t;

/**
 * @brief Wakes up the MPU6050
 * * @param dev_handle I2C device handle
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mpu6050_init(i2c_master_dev_handle_t dev_handle);

/**
 * @brief Reads 14 bytes of accel, temp, and gyro data
 * * @param dev_handle I2C device handle
 * @param data Pointer to struct to store the readings
 * @return esp_err_t ESP_OK on success
 */
esp_err_t mpu6050_read_data(i2c_master_dev_handle_t dev_handle, mpu6050_data_t *data);