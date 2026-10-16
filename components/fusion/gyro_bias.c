#include "gyro_bias.h"

#include <math.h>
#include <string.h>
#include "vec3.h"

void gyro_bias_init(gyro_bias_t *b, float still_gyro_dps, float still_accel_g, float still_time_s, float track_tau_s)
{
    memset(b, 0, sizeof(*b));
    b->still_gyro_dps = still_gyro_dps;
    b->still_accel_g = still_accel_g;
    b->still_time_s = still_time_s;
    b->track_tau_s = track_tau_s;
}

void gyro_bias_set(gyro_bias_t *b, const float bias[3])
{
    memcpy(b->bias, bias, sizeof(b->bias));
}

bool gyro_bias_update(gyro_bias_t *b, const float gyro_dps[3], const float accel_g[3], float dt)
{
    const float d[3] = { gyro_dps[0] - b->bias[0], gyro_dps[1] - b->bias[1], gyro_dps[2] - b->bias[2] };
    const float a = v3_norm(accel_g);

    // Wider gyro gate during a forced capture: the bias itself may be large
    const float gate = b->capturing ? b->still_gyro_dps * 4.0f : b->still_gyro_dps;
    const bool quiet = v3_norm(d) < gate && fabsf(a - 1.0f) < b->still_accel_g;

    if (quiet) {
        b->still_for_s += dt;
    } else {
        b->still_for_s = 0.0f;
    }
    b->still = b->still_for_s >= b->still_time_s;

    if (b->capturing) {
        if (!quiet) {
            b->cap_n = 0;
            b->cap_sum[0] = b->cap_sum[1] = b->cap_sum[2] = 0.0f;
        } else {
            for (int i = 0; i < 3; i++) {
                b->cap_sum[i] += gyro_dps[i];
            }
            b->cap_n++;
        }
    } else if (b->still && b->track_tau_s > 0.0f) {
        const float k = dt / (b->track_tau_s + dt);
        for (int i = 0; i < 3; i++) {
            b->bias[i] += k * (gyro_dps[i] - b->bias[i]);
        }
    }
    return b->still;
}

void gyro_bias_capture_start(gyro_bias_t *b, int n)
{
    b->capturing = true;
    b->cap_n = 0;
    b->cap_target = n;
    b->cap_sum[0] = b->cap_sum[1] = b->cap_sum[2] = 0.0f;
}

int gyro_bias_capture_poll(gyro_bias_t *b)
{
    if (!b->capturing) {
        return 1;
    }
    if (b->cap_n < b->cap_target) {
        return 0;
    }
    for (int i = 0; i < 3; i++) {
        b->bias[i] = b->cap_sum[i] / (float)b->cap_n;
    }
    b->capturing = false;
    return 1;
}
