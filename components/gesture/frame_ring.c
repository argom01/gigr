#include "frame_ring.h"

#include <stdatomic.h>
#include <string.h>

#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define RING_ATTR EXT_RAM_BSS_ATTR     // 11 kB, keep it out of internal RAM
#else
#define RING_ATTR
#endif

// Readers do not lock: they check that their frames are written and not yet
// reused before copying, and check again afterwards (seqlock-style). If the
// writer lapped them during the copy the data may be torn, so it is rejected.

RING_ATTR static float s_frames[FRAME_RING_LEN][GESTURE_AXES];
static atomic_uint s_count;

void frame_ring_push(const float frame[GESTURE_AXES])
{
    const uint32_t n = atomic_load_explicit(&s_count, memory_order_relaxed);
    // the previous count update is visible before this slot is overwritten
    atomic_thread_fence(memory_order_release);
    memcpy(s_frames[n % FRAME_RING_LEN], frame, sizeof(s_frames[0]));
    atomic_store_explicit(&s_count, n + 1, memory_order_release);
}

uint32_t frame_ring_count(void)
{
    return atomic_load_explicit(&s_count, memory_order_acquire);
}

// Frame `first` is intact while the writer, which may be filling slot `count`
// right now, is less than a full lap ahead of it
static bool intact(uint32_t first, uint32_t count)
{
    return count - first <= FRAME_RING_LEN - 1;
}

// Before a copy: frames [first, end) are written and intact
static bool readable(uint32_t first, uint32_t end)
{
    const uint32_t count = frame_ring_count();   // acquire: the slots below it are complete
    return end <= count && intact(first, count);
}

// After a copy: the writer did not reach the copied slots meanwhile
static bool not_lapped(uint32_t first)
{
    atomic_thread_fence(memory_order_acquire);   // the copy is done before the re-check
    return intact(first, atomic_load_explicit(&s_count, memory_order_relaxed));
}

bool frame_ring_get(uint32_t idx, float out[GESTURE_AXES])
{
    if (!readable(idx, idx + 1)) {
        return false;
    }
    memcpy(out, s_frames[idx % FRAME_RING_LEN], sizeof(s_frames[0]));
    return not_lapped(idx);
}

bool frame_ring_window(uint32_t end, int n, float *out)
{
    if (n <= 0 || (uint32_t)n > end || (uint32_t)n > FRAME_RING_LEN - 1) {
        return false;
    }
    const uint32_t first = end - (uint32_t)n;
    if (!readable(first, end)) {
        return false;
    }
    // at most two contiguous pieces (the window may wrap around the end)
    const uint32_t slot = first % FRAME_RING_LEN;
    const uint32_t head = (uint32_t)n < FRAME_RING_LEN - slot ? (uint32_t)n : FRAME_RING_LEN - slot;
    memcpy(out, s_frames[slot], head * sizeof(s_frames[0]));
    memcpy(out + head * GESTURE_AXES, s_frames[0], ((uint32_t)n - head) * sizeof(s_frames[0]));
    return not_lapped(first);
}
