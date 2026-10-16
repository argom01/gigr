#ifndef FLEX_ADC_H
#define FLEX_ADC_H

#include "board.h"
#include "esp_err.h"

// ADC1 continuous (DMA) sampling of the 5 flex sensors and the battery
// divider. Values are averaged over everything converted since the last poll.
esp_err_t flex_adc_init(void);

// Non-blocking. Returns the number of samples averaged per finger
// (0 = nothing new, outputs keep their previous values).
int flex_adc_poll(float finger_mv[FINGER_COUNT], float *bat_adc_mv);

#endif
