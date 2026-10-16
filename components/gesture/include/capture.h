#ifndef CAPTURE_H
#define CAPTURE_H

#include <stdbool.h>
#include "esp_err.h"

// Data capture for Edge Impulse: streams the classifier input frames as CSV
// lines (11 values, 100 Hz) on the USB console for
// `edge-impulse-data-forwarder`. Logging is muted while streaming.

esp_err_t capture_init(void);
void capture_start(void);
void capture_stop(void);
bool capture_running(void);

#endif
