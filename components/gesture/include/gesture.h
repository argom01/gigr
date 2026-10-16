#ifndef GESTURE_H
#define GESTURE_H

#include <stdbool.h>
#include <stdint.h>

// Pure header: the gesture dictionary expanded from gesture_defs.h.

typedef enum {
    G_KIND_IDLE = 0,
    G_KIND_DYNAMIC,
    G_KIND_STATIC,
} gesture_kind_t;

// names used in the kind column of gesture_defs.h
#define K_IDLE   G_KIND_IDLE
#define K_DYN    G_KIND_DYNAMIC
#define K_STATIC G_KIND_STATIC

typedef enum {
#define GESTURE(id, label, kind, min_prob, hold_ms) id,
#include "gesture_defs.h"
#undef GESTURE
    GESTURE_COUNT
} gesture_id_t;

typedef struct {
    const char *label;
    gesture_kind_t kind;
    float min_prob;
    uint16_t hold_ms;
} gesture_info_t;

extern const gesture_info_t g_gestures[GESTURE_COUNT];

const char *gesture_name(int id);
int gesture_find(const char *label);    // -1 if unknown

// ML input frame: 11 channels at 100 Hz, 1 s window
#define GESTURE_AXES 11

// Builds one normalised input frame. Used by both the data capture and the
// classifier, so training and inference always see identical scaling:
//   [ax ay az]/2 g, [gx gy gz]/500 dps, flex 0..1 (thumb index middle ring pinky)
void gesture_frame_build(const float accel_g[3], const float gyro_dps[3], const float flex[5],
                         float out[GESTURE_AXES]);

#endif
