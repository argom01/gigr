#include <string.h>
#include "gesture.h"

const gesture_info_t g_gestures[GESTURE_COUNT] = {
#define GESTURE(id, label, kind, min_prob, hold_ms) [id] = { label, kind, min_prob, hold_ms },
#include "gesture_defs.h"
#undef GESTURE
};

_Static_assert(GESTURE_COUNT <= 16, "at most 16 gestures (APP_MAX_GESTURES)");

const char *gesture_name(int id)
{
    return id >= 0 && id < GESTURE_COUNT ? g_gestures[id].label : "?";
}

int gesture_find(const char *label)
{
    for (int i = 0; i < GESTURE_COUNT; i++) {
        if (strcmp(g_gestures[i].label, label) == 0) {
            return i;
        }
    }
    return -1;
}

void gesture_frame_build(const float accel_g[3], const float gyro_dps[3], const float flex[5],
                         float out[GESTURE_AXES])
{
    for (int i = 0; i < 3; i++) {
        out[i] = accel_g[i] / 2.0f;
        out[3 + i] = gyro_dps[i] / 500.0f;
    }
    for (int i = 0; i < 5; i++) {
        out[6 + i] = flex[i];
    }
}
