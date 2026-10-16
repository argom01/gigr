#ifndef FLEX_CAL_H
#define FLEX_CAL_H

#include <stdbool.h>
#include <stdint.h>
#include "board.h"

// Pure C (no ESP-IDF): flex normalisation and calibration helpers.
//
// norm = clamp((x - open) / (closed - open), 0, 1)
// The sign of (closed - open) absorbs the inverted thumb divider, so every
// finger reads 0 = straight, 1 = fully bent.

float flex_normalize(float x, float open, float closed);

// Divider output (mV) -> flex resistance (ohm); FLEX_LINEARIZE_RESISTANCE only
float flex_mv_to_ohm(float mv, int finger);

// Normalise one finger with the configured domain (mV or ohm)
float flex_normalize_finger(int finger, float mv, float open_mv, float closed_mv);

// Running mean / standard deviation (Welford)
typedef struct {
    uint32_t n;
    float mean;
    float m2;
} welford_t;

void welford_reset(welford_t *w);
void welford_add(welford_t *w, float x);
float welford_std(const welford_t *w);

// Bit i set when finger i has |closed - open| >= min_span_mv
uint8_t flex_cal_span_mask(const float open_mv[FINGER_COUNT], const float closed_mv[FINGER_COUNT],
                           float min_span_mv);
// Bit i set when the span of finger i has the sign expected from the wiring
uint8_t flex_cal_polarity_mask(const float open_mv[FINGER_COUNT], const float closed_mv[FINGER_COUNT]);

#define FLEX_ALL_FINGERS_MASK ((uint8_t)((1u << FINGER_COUNT) - 1))

#endif
