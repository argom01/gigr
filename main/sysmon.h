#ifndef SYSMON_H
#define SYSMON_H

#include "esp_err.h"

// Low priority housekeeping task (core 0): status LED, backlight auto-dim,
// battery level, CPU load / heap statistics and debounced settings writes.
esp_err_t sysmon_start(void);

#endif
