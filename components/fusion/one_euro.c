#include "one_euro.h"

#include <math.h>

static float alpha(float cutoff, float dt)
{
    const float tau = 1.0f / (2.0f * (float)M_PI * cutoff);
    return 1.0f / (1.0f + tau / dt);
}

void one_euro_init(one_euro_t *f, float mincutoff, float beta, float dcutoff)
{
    f->mincutoff = mincutoff;
    f->beta = beta;
    f->dcutoff = dcutoff;
    one_euro_reset(f);
}

void one_euro_reset(one_euro_t *f)
{
    f->x_prev = 0.0f;
    f->dx_prev = 0.0f;
    f->init = false;
}

float one_euro_filter(one_euro_t *f, float x, float dt)
{
    if (!f->init || dt <= 0.0f) {
        f->init = true;
        f->x_prev = x;
        f->dx_prev = 0.0f;
        return x;
    }
    const float dx = (x - f->x_prev) / dt;
    const float a_d = alpha(f->dcutoff, dt);
    const float dx_hat = a_d * dx + (1.0f - a_d) * f->dx_prev;
    const float cutoff = f->mincutoff + f->beta * fabsf(dx_hat);
    const float a = alpha(cutoff, dt);
    const float x_hat = a * x + (1.0f - a) * f->x_prev;
    f->x_prev = x_hat;
    f->dx_prev = dx_hat;
    return x_hat;
}
