// Real Edge Impulse classifier glue (compiled only when the exported C++
// library is present in this folder: edge-impulse-sdk/, model-parameters/,
// tflite-model/).

#include "ei_glue.h"

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "esp_timer.h"

extern "C" bool eim_is_real(void)
{
    return true;
}

extern "C" int eim_label_count(void)
{
    return EI_CLASSIFIER_LABEL_COUNT;
}

extern "C" const char *eim_label(int i)
{
    return (i >= 0 && i < EI_CLASSIFIER_LABEL_COUNT) ? ei_classifier_inferencing_categories[i] : "";
}

extern "C" int eim_window_samples(void)
{
    return EI_CLASSIFIER_RAW_SAMPLE_COUNT;
}

extern "C" int eim_axes(void)
{
    return EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
}

extern "C" int eim_frequency_hz(void)
{
    return (int)EI_CLASSIFIER_FREQUENCY;
}

extern "C" int eim_classify(const float *window, int n_values, float *scores, int max_scores, uint32_t *time_us)
{
    if (n_values != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        return -1;
    }
    signal_t signal;
    if (numpy::signal_from_buffer(const_cast<float *>(window), (size_t)n_values, &signal) != 0) {
        return -2;
    }
    ei_impulse_result_t result = {};
    const int64_t t0 = esp_timer_get_time();
    const EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
    if (time_us) {
        *time_us = (uint32_t)(esp_timer_get_time() - t0);
    }
    if (err != EI_IMPULSE_OK) {
        return -3;
    }
    const int n = EI_CLASSIFIER_LABEL_COUNT < max_scores ? EI_CLASSIFIER_LABEL_COUNT : max_scores;
    for (int i = 0; i < n; i++) {
        scores[i] = result.classification[i].value;
    }
    return n;
}
