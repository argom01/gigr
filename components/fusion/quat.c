#include "quat.h"

#include <math.h>
#include "vec3.h"

void quat_identity(float q[4])
{
    q[0] = 1.0f;
    q[1] = q[2] = q[3] = 0.0f;
}

void quat_normalize(float q[4])
{
    const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n < 1e-9f) {
        quat_identity(q);
        return;
    }
    for (int i = 0; i < 4; i++) {
        q[i] /= n;
    }
}

void quat_mul(const float a[4], const float b[4], float out[4])
{
    const float w = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    const float x = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    const float y = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    const float z = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
    out[0] = w;
    out[1] = x;
    out[2] = y;
    out[3] = z;
}

void quat_conj(const float q[4], float out[4])
{
    out[0] = q[0];
    out[1] = -q[1];
    out[2] = -q[2];
    out[3] = -q[3];
}

void quat_rotate(const float q[4], const float v[3], float out[3])
{
    // v' = v + 2w(u x v) + 2(u x (u x v)), u = (x, y, z)
    const float u[3] = { q[1], q[2], q[3] };
    float t[3];
    v3_cross(u, v, t);
    v3_scale(t, 2.0f);
    float ut[3];
    v3_cross(u, t, ut);
    out[0] = v[0] + q[0] * t[0] + ut[0];
    out[1] = v[1] + q[0] * t[1] + ut[1];
    out[2] = v[2] + q[0] * t[2] + ut[2];
}

void quat_from_two_vectors(const float from[3], const float to[3], float q[4])
{
    const float d = v3_dot(from, to);
    if (d < -0.999999f) {
        // opposite: rotate 180 deg about any axis perpendicular to `from`
        float axis[3];
        const float x_axis[3] = { 1.0f, 0.0f, 0.0f };
        const float y_axis[3] = { 0.0f, 1.0f, 0.0f };
        v3_cross(x_axis, from, axis);
        if (v3_norm(axis) < 1e-6f) {
            v3_cross(y_axis, from, axis);
        }
        v3_normalize(axis);
        q[0] = 0.0f;
        q[1] = axis[0];
        q[2] = axis[1];
        q[3] = axis[2];
        return;
    }
    float c[3];
    v3_cross(from, to, c);
    q[0] = 1.0f + d;
    q[1] = c[0];
    q[2] = c[1];
    q[3] = c[2];
    quat_normalize(q);
}

void quat_to_euler_deg(const float q[4], float *roll, float *pitch, float *yaw)
{
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    *roll = RAD2DEG(atan2f(2.0f * (w * x + y * z), 1.0f - 2.0f * (x * x + y * y)));
    float s = 2.0f * (w * y - z * x);
    if (s > 1.0f) {
        s = 1.0f;
    } else if (s < -1.0f) {
        s = -1.0f;
    }
    *pitch = RAD2DEG(asinf(s));
    *yaw = RAD2DEG(atan2f(2.0f * (w * z + x * y), 1.0f - 2.0f * (y * y + z * z)));
}

void quat_up_in_body(const float q[4], float up[3])
{
    // third row of the rotation matrix R(q) (body -> world)
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    up[0] = 2.0f * (x * z - w * y);
    up[1] = 2.0f * (y * z + w * x);
    up[2] = 1.0f - 2.0f * (x * x + y * y);
}
