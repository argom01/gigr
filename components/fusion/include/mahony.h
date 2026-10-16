#ifndef MAHONY_H
#define MAHONY_H

#include <stdbool.h>

// 6-axis Mahony complementary filter. q rotates body -> world (Z up).
typedef struct {
    float q[4];
    float integral[3];
    float kp;
    float ki;
    bool initialised;
} mahony_t;

void mahony_init(mahony_t *f, float kp, float ki);
void mahony_reset(mahony_t *f);

// gyro in rad/s (bias removed), accel in any unit (only its direction is used)
void mahony_update(mahony_t *f, const float gyro_rad[3], const float accel[3], float dt);

#endif
