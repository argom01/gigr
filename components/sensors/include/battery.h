#ifndef BATTERY_H
#define BATTERY_H

#include <stdbool.h>
#include <stdint.h>

// Pure C: single-cell LiPo state-of-charge estimate from voltage.

// Open-circuit-voltage lookup, 0..100 %
uint8_t battery_pct_from_mv(float mv);

typedef struct {
    float mv_filt;
    int pct_shown;
    bool init;
} battery_est_t;

// mv: battery voltage (after the divider ratio). Smooths the voltage,
// compensates the rise while charging and applies hysteresis to the shown %.
uint8_t battery_est_update(battery_est_t *b, float mv, bool charging, float dt);

#endif
