#ifndef SENSOR_TASK_H
#define SENSOR_TASK_H

#include "esp_err.h"

// 200 Hz acquisition loop on core 1: IMU + flex + battery, bias tracking,
// levelling, orientation filter, calibrations, air mouse and the gesture
// sample stream.
esp_err_t sensor_task_start(void);

#endif
