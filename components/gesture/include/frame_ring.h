#ifndef FRAME_RING_H
#define FRAME_RING_H

#include <stdbool.h>
#include <stdint.h>
#include "gesture.h"

// Single-writer broadcast ring of 100 Hz input frames (sensor task writes,
// the classifier and the data capture read independently).

#define FRAME_RING_LEN 256

void frame_ring_push(const float frame[GESTURE_AXES]);

// Total number of frames ever pushed
uint32_t frame_ring_count(void);

// Frame with absolute index idx; false if not yet written or already overwritten
bool frame_ring_get(uint32_t idx, float out[GESTURE_AXES]);

// The n frames ending just before `end` (oldest first); false if unavailable
bool frame_ring_window(uint32_t end, int n, float *out);

#endif
