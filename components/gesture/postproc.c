#include "postproc.h"

#include <string.h>

void postproc_init(postproc_t *pp, float th_offset, int refractory_ms)
{
    memset(pp, 0, sizeof(*pp));
    pp->th_offset = th_offset;
    pp->refractory_ms = refractory_ms;
}

void postproc_reset(postproc_t *pp)
{
    postproc_init(pp, pp->th_offset, pp->refractory_ms);
}

static int popcount3(uint8_t v)
{
    return (v & 1) + ((v >> 1) & 1) + ((v >> 2) & 1);
}

int postproc_update(postproc_t *pp, const float *scores, int64_t t_us, float dt_ms)
{
    int top = 0;
    for (int i = 1; i < GESTURE_COUNT; i++) {
        if (scores[i] > scores[top]) {
            top = i;
        }
    }
    if (pp->need_idle && g_gestures[top].kind == G_KIND_IDLE) {
        pp->need_idle = false;
    }

    int best = -1;
    float best_p = 0.0f;
    for (int i = 0; i < GESTURE_COUNT; i++) {
        const gesture_info_t *g = &g_gestures[i];
        if (g->kind == G_KIND_IDLE) {
            continue;
        }
        const bool above = scores[i] >= g->min_prob + pp->th_offset;
        pp->hist[i] = (uint8_t)(((pp->hist[i] << 1) | (above ? 1 : 0)) & 0x7);

        bool ready = false;
        if (g->kind == G_KIND_DYNAMIC) {
            ready = !pp->need_idle && popcount3(pp->hist[i]) >= 2;
        } else {
            if (above) {
                pp->held_ms[i] += dt_ms;
            } else {
                pp->held_ms[i] = 0.0f;
                pp->latched[i] = false;
            }
            ready = !pp->latched[i] && pp->held_ms[i] >= (float)g->hold_ms;
        }
        if (ready && scores[i] > best_p) {
            best = i;
            best_p = scores[i];
        }
    }

    if (best < 0 || t_us < pp->refractory_until_us) {
        return -1;
    }

    pp->refractory_until_us = t_us + (int64_t)pp->refractory_ms * 1000;
    memset(pp->hist, 0, sizeof(pp->hist));
    if (g_gestures[best].kind == G_KIND_DYNAMIC) {
        pp->need_idle = true;
    } else {
        pp->latched[best] = true;
    }
    return best;
}
