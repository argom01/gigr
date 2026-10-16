#ifndef ONE_EURO_H
#define ONE_EURO_H

#include <stdbool.h>

// 1-euro filter (Casiez et al. 2012): low cut-off (less jitter) at low speed,
// higher cut-off (less lag) at high speed.
typedef struct {
    float mincutoff;    // Hz
    float beta;         // cut-off increase per unit of speed
    float dcutoff;      // Hz, for the derivative
    float x_prev;
    float dx_prev;
    bool init;
} one_euro_t;

void one_euro_init(one_euro_t *f, float mincutoff, float beta, float dcutoff);
void one_euro_reset(one_euro_t *f);
float one_euro_filter(one_euro_t *f, float x, float dt);

#endif
