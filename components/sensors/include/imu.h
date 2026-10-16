#ifndef IMU_H
#define IMU_H

#include "esp_err.h"

// MPU6500 on the shared I2C bus: +-4 g, +-1000 dps, DLPF ~41 Hz, 200 Hz ODR.
// Returns ESP_ERR_NOT_FOUND when the chip does not answer.
esp_err_t imu_init(void);

// One 14-byte burst read. Sensor frame (before the mounting rotation).
esp_err_t imu_read(float accel_g[3], float gyro_dps[3], float *temp_c);

#endif
