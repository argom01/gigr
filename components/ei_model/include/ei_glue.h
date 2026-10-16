#ifndef EI_GLUE_H
#define EI_GLUE_H

#include <stdbool.h>
#include <stdint.h>

// Thin C interface to the Edge Impulse classifier. Until a model is exported
// into this component a stub is built that reports no labels.

#ifdef __cplusplus
extern "C" {
#endif

bool eim_is_real(void);
int eim_label_count(void);
const char *eim_label(int i);
int eim_window_samples(void);   // samples per window (per axis)
int eim_axes(void);             // values per sample
int eim_frequency_hz(void);

// window: eim_window_samples() * eim_axes() floats, interleaved per sample.
// Writes eim_label_count() scores; returns that count, or < 0 on error.
int eim_classify(const float *window, int n_values, float *scores, int max_scores, uint32_t *time_us);

#ifdef __cplusplus
}
#endif

#endif
