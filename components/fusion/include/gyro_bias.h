#ifndef GYRO_BIAS_H
#define GYRO_BIAS_H

#include <stdbool.h>

// Stillness detector + slow gyro bias tracker. Bias is estimated in whatever
// frame the samples are given in (the sensor frame in this firmware).
typedef struct {
    float bias[3];
    float still_gyro_dps;       // |gyro - bias| below this ...
    float still_accel_g;        // ... and | |a| - 1 g | below this ...
    float still_time_s;         // ... for this long = still
    float track_tau_s;          // bias low-pass time constant while still
    float still_for_s;
    bool still;

    // forced capture (calibration button)
    bool capturing;
    float cap_sum[3];
    int cap_n;
    int cap_target;
} gyro_bias_t;

void gyro_bias_init(gyro_bias_t *b, float still_gyro_dps, float still_accel_g, float still_time_s, float track_tau_s);
void gyro_bias_set(gyro_bias_t *b, const float bias[3]);

// Feed one sample (gyro in dps, accel in g). Returns true while still.
bool gyro_bias_update(gyro_bias_t *b, const float gyro_dps[3], const float accel_g[3], float dt);

// Forced capture of n samples; the capture restarts whenever motion is seen
void gyro_bias_capture_start(gyro_bias_t *b, int n);
// 0 = running, 1 = finished (bias updated)
int gyro_bias_capture_poll(gyro_bias_t *b);

#endif
