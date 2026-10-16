#include <string.h>
#include "frame_ring.h"
#include "gesture.h"
#include "postproc.h"
#include "profiles.h"
#include "test.h"

#define MS(x) ((int64_t)(x) * 1000)

static void scores_for(float *s, int id, float p)
{
    for (int i = 0; i < GESTURE_COUNT; i++) {
        s[i] = 0.0f;
    }
    s[G_IDLE] = 1.0f - p;
    s[id] = p;
}

static void test_dictionary(void)
{
    CHECK(GESTURE_COUNT >= 8 && GESTURE_COUNT <= 16);
    CHECK(gesture_find("swipe_left") == G_SWIPE_LEFT);
    CHECK(gesture_find("thumbs_up") == G_THUMBS_UP);
    CHECK(gesture_find("nope") == -1);
    CHECK(g_gestures[G_IDLE].kind == G_KIND_IDLE);
    CHECK(g_gestures[G_SWIPE_UP].kind == G_KIND_DYNAMIC);
    CHECK(g_gestures[G_FIST].kind == G_KIND_STATIC);
    int idle = 0;
    for (int i = 0; i < GESTURE_COUNT; i++) {
        idle += g_gestures[i].kind == G_KIND_IDLE;
        CHECK(g_gestures[i].label && g_gestures[i].label[0]);
    }
    CHECK(idle == 1);
}

static void test_frame_scaling(void)
{
    const float a[3] = { 0, 0, 1 }, g[3] = { 250, -500, 0 }, f[5] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f };
    float out[GESTURE_AXES];
    gesture_frame_build(a, g, f, out);
    CHECK_NEAR(out[2], 0.5f, 1e-6);
    CHECK_NEAR(out[3], 0.5f, 1e-6);
    CHECK_NEAR(out[4], -1.0f, 1e-6);
    CHECK_NEAR(out[6], 0.1f, 1e-6);
    CHECK_NEAR(out[10], 0.5f, 1e-6);
}

static void test_dynamic(void)
{
    postproc_t pp;
    postproc_init(&pp, 0.0f, 600);
    float s[GESTURE_COUNT];
    int64_t t = MS(1000);

    scores_for(s, G_SWIPE_LEFT, 0.95f);
    CHECK(postproc_update(&pp, s, t, 100) == -1);       // 1 of 3
    t += MS(100);
    CHECK(postproc_update(&pp, s, t, 100) == G_SWIPE_LEFT);
    // still high: locked out until idle, even after the refractory time
    for (int i = 0; i < 10; i++) {
        t += MS(100);
        CHECK(postproc_update(&pp, s, t, 100) == -1);
    }
    scores_for(s, G_IDLE, 0.0f);
    t += MS(100);
    CHECK(postproc_update(&pp, s, t, 100) == -1);
    scores_for(s, G_SWIPE_RIGHT, 0.9f);
    t += MS(100);
    postproc_update(&pp, s, t, 100);
    t += MS(100);
    CHECK(postproc_update(&pp, s, t, 100) == G_SWIPE_RIGHT);
}

static void test_refractory(void)
{
    postproc_t pp;
    postproc_init(&pp, 0.0f, 600);
    float s[GESTURE_COUNT], idle[GESTURE_COUNT];
    scores_for(idle, G_IDLE, 0.0f);
    int64_t t = MS(1000);
    scores_for(s, G_SWIPE_UP, 0.9f);
    postproc_update(&pp, s, t, 100);
    t += MS(100);
    CHECK(postproc_update(&pp, s, t, 100) == G_SWIPE_UP);
    t += MS(100);
    postproc_update(&pp, idle, t, 100);
    // a new swipe right away is blocked by the refractory period
    scores_for(s, G_SWIPE_DOWN, 0.9f);
    t += MS(100);
    postproc_update(&pp, s, t, 100);
    t += MS(100);
    CHECK(postproc_update(&pp, s, t, 100) == -1);
    t += MS(400);
    CHECK(postproc_update(&pp, s, t, 100) == G_SWIPE_DOWN);
}

static void test_static_hold(void)
{
    postproc_t pp;
    postproc_init(&pp, 0.0f, 300);
    float s[GESTURE_COUNT];
    scores_for(s, G_FIST, 0.95f);
    int64_t t = MS(1000);
    int fired = 0, first = -1;
    for (int i = 0; i < 30; i++) {          // 3 s of holding
        t += MS(100);
        if (postproc_update(&pp, s, t, 100) == G_FIST) {
            fired++;
            if (first < 0) {
                first = i;
            }
        }
    }
    CHECK(fired == 1);                      // once per hold
    CHECK(first >= 3 && first <= 4);        // after hold_ms = 400
    scores_for(s, G_IDLE, 0.0f);
    t += MS(100);
    postproc_update(&pp, s, t, 100);
    scores_for(s, G_FIST, 0.95f);
    for (int i = 0; i < 6; i++) {
        t += MS(100);
        fired += postproc_update(&pp, s, t, 100) == G_FIST;
    }
    CHECK(fired == 2);
}

static void test_threshold_offset(void)
{
    postproc_t pp;
    postproc_init(&pp, 0.15f, 600);
    float s[GESTURE_COUNT];
    scores_for(s, G_SWIPE_LEFT, 0.9f);      // below 0.80 + 0.15
    int64_t t = MS(1000);
    for (int i = 0; i < 5; i++) {
        t += MS(100);
        CHECK(postproc_update(&pp, s, t, 100) == -1);
    }
}

static void test_profiles(void)
{
    CHECK(g_profile_count >= 6);
    char buf[48];
    action_describe(profile_action(0, G_SWIPE_LEFT), buf, sizeof(buf));
    CHECK(strcmp(buf, "Ctrl+Win/Cmd+Left") == 0);
    action_describe(profile_action(2, G_SWIPE_UP), buf, sizeof(buf));
    CHECK(strcmp(buf, "Win/Cmd") == 0);
    action_describe(profile_action(5, G_FIST), buf, sizeof(buf));
    CHECK(strcmp(buf, "Play/Pause") == 0);
    // idle never maps to anything, out-of-range lookups are safe
    for (int p = 0; p < g_profile_count; p++) {
        CHECK(profile_action(p, G_IDLE)->type == ACT_NONE);
        CHECK(g_profiles[p].name != NULL);
    }
    CHECK(profile_action(99, G_FIST)->type == ACT_NONE);
    CHECK(profile_action(0, 99)->type == ACT_NONE);
    // dropdown has one line per profile
    const char *opts = profile_dropdown_options();
    int lines = 1;
    for (const char *c = opts; *c; c++) {
        lines += *c == '\n';
    }
    CHECK(lines == g_profile_count);
}

static void test_frame_ring(void)
{
    float f[GESTURE_AXES];
    for (int i = 0; i < 300; i++) {
        for (int k = 0; k < GESTURE_AXES; k++) {
            f[k] = (float)(i * 100 + k);
        }
        frame_ring_push(f);
    }
    CHECK(frame_ring_count() == 300);
    static float win[100 * GESTURE_AXES];
    CHECK(frame_ring_window(300, 100, win));
    CHECK_NEAR(win[0], 200 * 100, 1e-3);                        // oldest = frame 200
    CHECK_NEAR(win[99 * GESTURE_AXES + 10], 299 * 100 + 10, 1e-3);
    CHECK(!frame_ring_get(10, f));                              // overwritten
    CHECK(!frame_ring_get(300, f));                             // not written yet
    CHECK(frame_ring_get(299, f));
}

int main(void)
{
    RUN(test_dictionary);
    RUN(test_frame_scaling);
    RUN(test_dynamic);
    RUN(test_refractory);
    RUN(test_static_hold);
    RUN(test_threshold_offset);
    RUN(test_profiles);
    RUN(test_frame_ring);
    TEST_MAIN_END();
}
