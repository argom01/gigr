#ifndef GESTURE_TASK_H
#define GESTURE_TASK_H

#include "esp_err.h"
#include "gesture.h"

// Classifier task (core 1): every GESTURE_STRIDE_SAMPLES new frames it runs
// the Edge Impulse model on the last 1 s, post-processes the scores and sends
// the action of the active host profile.
esp_err_t gesture_start(void);

// 100 Hz input from the sensor task
void gesture_feed(const float frame[GESTURE_AXES]);

// Send the mapped action of a gesture without the model (GUI test button)
void gesture_fire_test(int id);

// Remember the current profile for the connected host
void gesture_bind_profile_to_peer(void);

// Effective profile for the connected host (per-host binding or the default)
int gesture_active_profile(void);

// Optional extra consumer of every frame (data capture)
typedef void (*gesture_frame_cb_t)(uint32_t frame_index);
void gesture_set_frame_listener(gesture_frame_cb_t cb);

#endif
