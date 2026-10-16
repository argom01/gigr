#ifndef VEC3_H
#define VEC3_H

#include <math.h>

static inline float v3_dot(const float a[3], const float b[3])
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline void v3_cross(const float a[3], const float b[3], float out[3])
{
    const float x = a[1] * b[2] - a[2] * b[1];
    const float y = a[2] * b[0] - a[0] * b[2];
    const float z = a[0] * b[1] - a[1] * b[0];
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

static inline float v3_norm(const float a[3])
{
    return sqrtf(v3_dot(a, a));
}

// Returns the original length; leaves the vector untouched if it is ~zero
static inline float v3_normalize(float a[3])
{
    const float n = v3_norm(a);
    if (n > 1e-9f) {
        a[0] /= n;
        a[1] /= n;
        a[2] /= n;
    }
    return n;
}

static inline void v3_scale(float a[3], float s)
{
    a[0] *= s;
    a[1] *= s;
    a[2] *= s;
}

// out = M * v, M row-major 3x3
static inline void m3_mul_v3(const float m[3][3], const float v[3], float out[3])
{
    const float x = m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2];
    const float y = m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2];
    const float z = m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2];
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

#define DEG2RAD(x) ((x) * 0.017453292519943295f)
#define RAD2DEG(x) ((x) * 57.29577951308232f)

#endif
