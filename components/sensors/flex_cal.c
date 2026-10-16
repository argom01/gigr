#include "flex_cal.h"

#include <math.h>
#include "board_config.h"

float flex_normalize(float x, float open, float closed)
{
    const float span = closed - open;
    if (fabsf(span) < 1e-6f) {
        return 0.0f;
    }
    float n = (x - open) / span;
    if (n < 0.0f) {
        n = 0.0f;
    } else if (n > 1.0f) {
        n = 1.0f;
    }
    return n;
}

float flex_mv_to_ohm(float mv, int finger)
{
    static const float r_fixed[FINGER_COUNT] = FLEX_FIXED_R_OHM;
    static const uint8_t rises[FINGER_COUNT] = FINGER_VOLTAGE_RISES_ON_BEND;
    const float vdd = FLEX_VDD_MV;
    if (mv < 1.0f) {
        mv = 1.0f;
    } else if (mv > vdd - 1.0f) {
        mv = vdd - 1.0f;
    }
    if (rises[finger]) {
        // fixed resistor to VDD, flex to GND: V = Vdd * Rf / (Rf + Rfix)
        return r_fixed[finger] * mv / (vdd - mv);
    }
    // flex to VDD, fixed resistor to GND: V = Vdd * Rfix / (Rf + Rfix)
    return r_fixed[finger] * (vdd / mv - 1.0f);
}

float flex_normalize_finger(int finger, float mv, float open_mv, float closed_mv)
{
#if FLEX_LINEARIZE_RESISTANCE
    return flex_normalize(flex_mv_to_ohm(mv, finger), flex_mv_to_ohm(open_mv, finger),
                          flex_mv_to_ohm(closed_mv, finger));
#else
    (void)finger;
    return flex_normalize(mv, open_mv, closed_mv);
#endif
}

void welford_reset(welford_t *w)
{
    w->n = 0;
    w->mean = 0.0f;
    w->m2 = 0.0f;
}

void welford_add(welford_t *w, float x)
{
    w->n++;
    const float d = x - w->mean;
    w->mean += d / (float)w->n;
    w->m2 += d * (x - w->mean);
}

float welford_std(const welford_t *w)
{
    return w->n > 1 ? sqrtf(w->m2 / (float)(w->n - 1)) : 0.0f;
}

uint8_t flex_cal_span_mask(const float open_mv[FINGER_COUNT], const float closed_mv[FINGER_COUNT],
                           float min_span_mv)
{
    uint8_t mask = 0;
    for (int i = 0; i < FINGER_COUNT; i++) {
        if (fabsf(closed_mv[i] - open_mv[i]) >= min_span_mv) {
            mask |= (uint8_t)(1u << i);
        }
    }
    return mask;
}

uint8_t flex_cal_polarity_mask(const float open_mv[FINGER_COUNT], const float closed_mv[FINGER_COUNT])
{
    static const uint8_t rises[FINGER_COUNT] = FINGER_VOLTAGE_RISES_ON_BEND;
    uint8_t mask = 0;
    for (int i = 0; i < FINGER_COUNT; i++) {
        const bool up = closed_mv[i] > open_mv[i];
        if (up == (rises[i] != 0)) {
            mask |= (uint8_t)(1u << i);
        }
    }
    return mask;
}
