#ifndef IMU_H
#define IMU_H

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// MPU6500 on the shared I2C bus: +-4 g, +-1000 dps, DLPF ~41 Hz, 200 Hz ODR.
// Returns ESP_ERR_NOT_FOUND when the chip does not answer.
esp_err_t imu_init(void);

// One 14-byte burst read. Sensor frame (before the mounting rotation).
esp_err_t imu_read(float accel_g[3], float gyro_dps[3], float *temp_c);

// Data-ready interrupt: the IMU pulses IMU_INT_PIN for every new sample; the
// ISR timestamps the edge and notifies `task` (one notification per sample).
esp_err_t imu_drdy_start(TaskHandle_t task);

// esp_timer time of the latest data-ready edge (the sample's timestamp)
int64_t imu_drdy_time_us(void);

#endif
