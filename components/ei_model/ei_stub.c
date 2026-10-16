#include "ei_glue.h"

// Built while no Edge Impulse export is present in this folder.

bool eim_is_real(void)
{
    return false;
}

int eim_label_count(void)
{
    return 0;
}

const char *eim_label(int i)
{
    (void)i;
    return "";
}

int eim_window_samples(void)
{
    return 0;
}

int eim_axes(void)
{
    return 0;
}

int eim_frequency_hz(void)
{
    return 0;
}

int eim_classify(const float *window, int n_values, float *scores, int max_scores, uint32_t *time_us)
{
    (void)window;
    (void)n_values;
    (void)scores;
    (void)max_scores;
    if (time_us) {
        *time_us = 0;
    }
    return -1;
}
