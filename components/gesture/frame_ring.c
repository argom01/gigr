#include "frame_ring.h"

#include <stdatomic.h>
#include <string.h>

static float s_frames[FRAME_RING_LEN][GESTURE_AXES];
static atomic_uint s_count;

void frame_ring_push(const float frame[GESTURE_AXES])
{
    const uint32_t n = atomic_load_explicit(&s_count, memory_order_relaxed);
    memcpy(s_frames[n % FRAME_RING_LEN], frame, sizeof(s_frames[0]));
    atomic_store_explicit(&s_count, n + 1, memory_order_release);
}

uint32_t frame_ring_count(void)
{
    return atomic_load_explicit(&s_count, memory_order_acquire);
}

bool frame_ring_get(uint32_t idx, float out[GESTURE_AXES])
{
    const uint32_t n = frame_ring_count();
    // keep a margin of a few frames to the writer
    if (idx >= n || n - idx > FRAME_RING_LEN - 8) {
        return false;
    }
    memcpy(out, s_frames[idx % FRAME_RING_LEN], sizeof(s_frames[0]));
    return true;
}

bool frame_ring_window(uint32_t end, int n, float *out)
{
    if ((uint32_t)n > end) {
        return false;
    }
    for (int i = 0; i < n; i++) {
        if (!frame_ring_get(end - (uint32_t)n + (uint32_t)i, out + i * GESTURE_AXES)) {
            return false;
        }
    }
    return true;
}
