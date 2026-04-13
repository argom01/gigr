#include "mpu6050.h"

#define MPU6050_PWR_MGMT_1   0x6B
#define MPU6050_ACCEL_XOUT_H 0x3B

#define MPU6050_I2C_ADDR 0x68

esp_err_t mpu6050_init(i2c_master_bus_handle_t bus_handle, i2c_master_dev_handle_t *dev_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_I2C_ADDR,
        .scl_speed_hz = 400000,
    };

    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, dev_handle));

    // Write 0x00 to PWR_MGMT_1 to wake up the MPU6050 from sleep mode
    uint8_t write_buf[2] = {MPU6050_PWR_MGMT_1, 0x00};

    return i2c_master_transmit(*dev_handle, write_buf, sizeof(write_buf), -1);
}

esp_err_t mpu6050_read_data(i2c_master_dev_handle_t dev_handle, mpu6050_data_t *data) {
    uint8_t reg = MPU6050_ACCEL_XOUT_H;
    uint8_t raw_data[14];

    esp_err_t err = i2c_master_transmit_receive(dev_handle, &reg, 1, raw_data, sizeof(raw_data), -1);

    if (err == ESP_OK) {
        data->accel_x = (raw_data[0] << 8) | raw_data[1];
        data->accel_y = (raw_data[2] << 8) | raw_data[3];
        data->accel_z = (raw_data[4] << 8) | raw_data[5];
        data->temp    = (raw_data[6] << 8) | raw_data[7];
        data->gyro_x  = (raw_data[8] << 8) | raw_data[9];
        data->gyro_y  = (raw_data[10] << 8) | raw_data[11];
        data->gyro_z  = (raw_data[12] << 8) | raw_data[13];
    }

    return err;
}