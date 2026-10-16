#include "mahony.h"

#include <math.h>
#include "quat.h"
#include "vec3.h"

void mahony_init(mahony_t *f, float kp, float ki)
{
    f->kp = kp;
    f->ki = ki;
    mahony_reset(f);
}

void mahony_reset(mahony_t *f)
{
    quat_identity(f->q);
    f->integral[0] = f->integral[1] = f->integral[2] = 0.0f;
    f->initialised = false;
}

void mahony_update(mahony_t *f, const float gyro_rad[3], const float accel[3], float dt)
{
    float a[3] = { accel[0], accel[1], accel[2] };
    const float a_norm = v3_normalize(a);
    float g[3] = { gyro_rad[0], gyro_rad[1], gyro_rad[2] };

    if (!f->initialised) {
        if (a_norm > 1e-6f) {
            const float up[3] = { 0.0f, 0.0f, 1.0f };
            quat_from_two_vectors(a, up, f->q);
            f->initialised = true;
        }
        return;
    }

    // Use the accelerometer only while it is a plausible gravity reading
    if (a_norm > 1e-6f) {
        float v[3];
        quat_up_in_body(f->q, v);
        float e[3];
        v3_cross(a, v, e);
        if (f->ki > 0.0f) {
            for (int i = 0; i < 3; i++) {
                f->integral[i] += f->ki * e[i] * dt;
                g[i] += f->integral[i];
            }
        }
        for (int i = 0; i < 3; i++) {
            g[i] += f->kp * e[i];
        }
    }

    // q += 0.5 * q * (0, g) * dt
    const float qg[4] = { 0.0f, g[0], g[1], g[2] };
    float dq[4];
    quat_mul(f->q, qg, dq);
    for (int i = 0; i < 4; i++) {
        f->q[i] += 0.5f * dq[i] * dt;
    }
    quat_normalize(f->q);
}
