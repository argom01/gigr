#ifndef I2C_BUS_H
#define I2C_BUS_H

#include "driver/i2c_master.h"
#include "esp_err.h"

// Shared I2C master bus (MPU6500 + GT911). The driver serialises transactions
// from different tasks.
esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_get(void);

#endif
