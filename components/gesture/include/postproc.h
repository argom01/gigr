#ifndef POSTPROC_H
#define POSTPROC_H

#include <stdbool.h>
#include <stdint.h>
#include "gesture.h"

// Pure C: turns per-window class probabilities into discrete gesture events.
//  - dynamic: p >= min_prob in 2 of the last 3 windows, then a refractory
//    period and a lockout until the idle class wins again
//  - static: p >= min_prob held for hold_ms, fires once per hold
typedef struct {
    float th_offset;            // added to every min_prob (GUI slider)
    int refractory_ms;
    uint8_t hist[GESTURE_COUNT];
    float held_ms[GESTURE_COUNT];
    bool latched[GESTURE_COUNT];
    int64_t refractory_until_us;
    bool need_idle;
} postproc_t;

void postproc_init(postproc_t *pp, float th_offset, int refractory_ms);
void postproc_reset(postproc_t *pp);

// scores[GESTURE_COUNT] in dictionary order; dt_ms since the previous window.
// Returns the gesture id that fired, or -1.
int postproc_update(postproc_t *pp, const float *scores, int64_t t_us, float dt_ms);

#endif
