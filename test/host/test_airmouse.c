#include <stdlib.h>
#include <string.h>
#include "airmouse.h"
#include "test.h"
#include "vec3.h"

#define DT 0.005f

static airmouse_params_t params(void)
{
    airmouse_params_t p;
    airmouse_params_default(&p);
    return p;
}

static const float UP_LEVEL[3] = { 0, 0, 1 };

static void flex_pose(float f[FINGER_COUNT], float index, float middle)
{
    f[FINGER_THUMB] = 0.2f;
    f[FINGER_INDEX] = index;
    f[FINGER_MIDDLE] = middle;
    f[FINGER_RING] = 0.9f;
    f[FINGER_PINKY] = 0.9f;
}

// Enter the mode and run past the settle time
static void enter(airmouse_t *am)
{
    float f[FINGER_COUNT];
    flex_pose(f, 0.1f, 0.1f);
    const float g[3] = { 0, 0, 0 };
    airmouse_out_t o;
    for (int i = 0; i < 100; i++) {
        airmouse_step(am, true, f, g, UP_LEVEL, DT, &o);
    }
    CHECK(o.active && !o.settling);
}

static void test_axes(void)
{
    const airmouse_params_t p = params();
    float x, y;
    const float yaw_left[3] = { 0, 0, 30 };     // CCW from above
    airmouse_screen_rates(&p, yaw_left, UP_LEVEL, &x, &y);
    CHECK_NEAR(x, -30, 1e-4);
    CHECK_NEAR(y, 0, 1e-4);
    const float tips_down[3] = { 0, 20, 0 };    // about +Y: fingertips go down
    airmouse_screen_rates(&p, tips_down, UP_LEVEL, &x, &y);
    CHECK_NEAR(x, 0, 1e-4);
    CHECK_NEAR(y, 20, 1e-4);
}

static void test_roll_compensation(void)
{
    // a turn about the world vertical gives the same cursor X whatever the roll
    const airmouse_params_t p = params();
    for (int deg = -80; deg <= 80; deg += 20) {
        const float r = DEG2RAD((float)deg);
        const float up[3] = { 0, sinf(r), cosf(r) };    // hand rolled about the forearm
        const float g[3] = { up[0] * 30, up[1] * 30, up[2] * 30 };
        float x, y;
        airmouse_screen_rates(&p, g, up, &x, &y);
        CHECK_NEAR(x, -30, 1e-3);
        CHECK_NEAR(y, 0, 1e-3);
    }
}

static void test_pose_enter_exit(void)
{
    airmouse_t am;
    airmouse_params_t p = params();
    airmouse_init(&am, &p);
    float f[FINGER_COUNT] = { 0 };
    const float g[3] = { 0 };
    airmouse_out_t o;
    // open hand: never active
    for (int i = 0; i < 200; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        CHECK(!o.active);
    }
    // curl ring + pinky: active after pose_ms
    flex_pose(f, 0, 0);
    int entered_at = -1;
    for (int i = 0; i < 100; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        if (o.entered) {
            entered_at = i;
        }
    }
    CHECK(entered_at >= (int)(p.pose_ms / 5) - 2 && entered_at <= (int)(p.pose_ms / 5) + 1);
    CHECK(o.active);
    // disabled from the GUI: leaves immediately
    airmouse_step(&am, false, f, g, UP_LEVEL, DT, &o);
    CHECK(o.exited && !o.active);
    // re-enter, then open the hand
    for (int i = 0; i < 100; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
    }
    f[FINGER_RING] = 0.2f;
    bool exited = false;
    for (int i = 0; i < 100; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        exited |= o.exited;
    }
    CHECK(exited && !o.active);
}

static int32_t sweep(float rate_dps, float total_deg, int axis)
{
    airmouse_t am;
    airmouse_params_t p = params();
    airmouse_init(&am, &p);
    enter(&am);
    float f[FINGER_COUNT];
    flex_pose(f, 0.1f, 0.1f);
    float g[3] = { 0, 0, 0 };
    g[axis] = rate_dps;
    airmouse_out_t o;
    int32_t sum = 0;
    const int n = (int)(total_deg / fabsf(rate_dps) / DT);
    for (int i = 0; i < n; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        sum += axis == 2 ? o.dx : o.dy;
    }
    // let the filter settle out
    g[axis] = 0;
    for (int i = 0; i < 200; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        sum += axis == 2 ? o.dx : o.dy;
    }
    return sum;
}

static void test_ballistics(void)
{
    const airmouse_params_t p = params();
    // 10 degrees slowly (wrist) vs quickly (arm)
    const int32_t slow = sweep(5.0f, 10.0f, 1);
    const int32_t fast = sweep(250.0f, 10.0f, 1);
    printf("   slow %d px, fast %d px\n", slow, fast);
    CHECK(slow > 0 && slow < (int32_t)(p.gain_min * 10 * 1.3f));
    CHECK(fast > (int32_t)(p.gain_max * 10 * 0.6f));
    CHECK(fast > 4 * slow);
    // turning right (clockwise from above, gyro z < 0) moves the cursor right
    CHECK(sweep(-60.0f, 20.0f, 2) > 0);
    CHECK(sweep(60.0f, 20.0f, 2) < 0);
}

static void test_deadzone(void)
{
    airmouse_t am;
    airmouse_params_t p = params();
    airmouse_init(&am, &p);
    enter(&am);
    float f[FINGER_COUNT];
    flex_pose(f, 0.1f, 0.1f);
    const float drift[3] = { 0.3f, 0.5f, 0.4f };
    airmouse_out_t o;
    int32_t sum = 0;
    for (int i = 0; i < 2000; i++) {
        airmouse_step(&am, true, f, drift, UP_LEVEL, DT, &o);
        sum += abs(o.dx) + abs(o.dy);
    }
    CHECK(sum == 0);
}

static void test_click_and_freeze(void)
{
    airmouse_t am;
    airmouse_params_t p = params();
    airmouse_init(&am, &p);
    enter(&am);
    float f[FINGER_COUNT];
    const float g[3] = { 0, 30, 0 };    // steady motion while clicking
    airmouse_out_t o;
    bool froze = false, pressed = false;
    int32_t moved_while_frozen = 0;
    for (int i = 0; i <= 20; i++) {     // index bends 0.1 -> 0.9 in 100 ms
        flex_pose(f, 0.1f + 0.04f * (float)i, 0.1f);
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        froze |= o.frozen;
        if (o.frozen) {
            moved_while_frozen += abs(o.dy);
        }
    }
    for (int i = 0; i < 20; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        pressed |= (o.buttons & 1) != 0;
        if (o.frozen) {
            moved_while_frozen += abs(o.dy);
        }
    }
    CHECK(froze);
    CHECK(pressed);
    CHECK(moved_while_frozen == 0);
    CHECK((o.buttons & 2) == 0);
    // after the freeze the drag moves again
    int32_t moved = 0;
    for (int i = 0; i < 100; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
        moved += o.dy;
    }
    CHECK(moved > 0 && (o.buttons & 1));
    // release
    flex_pose(f, 0.1f, 0.1f);
    for (int i = 0; i < 20; i++) {
        airmouse_step(&am, true, f, g, UP_LEVEL, DT, &o);
    }
    CHECK(o.buttons == 0);
}

static void test_scroll(void)
{
    airmouse_t am;
    airmouse_params_t p = params();
    airmouse_init(&am, &p);
    enter(&am);
    float f[FINGER_COUNT];
    flex_pose(f, 0.8f, 0.8f);           // both bent together
    const float still[3] = { 0, 0, 0 };
    airmouse_out_t o;
    for (int i = 0; i < 150; i++) {
        airmouse_step(&am, true, f, still, UP_LEVEL, DT, &o);
    }
    CHECK(o.scrolling);
    CHECK(o.buttons == 0);
    const float tips_down[3] = { 0, 40, 0 };
    int32_t wheel = 0, dy = 0;
    for (int i = 0; i < 200; i++) {
        airmouse_step(&am, true, f, tips_down, UP_LEVEL, DT, &o);
        wheel += o.wheel;
        dy += o.dy;
    }
    CHECK(wheel < 0);       // fingertips down = scroll down
    CHECK(dy == 0);
}

int main(void)
{
    RUN(test_axes);
    RUN(test_roll_compensation);
    RUN(test_pose_enter_exit);
    RUN(test_ballistics);
    RUN(test_deadzone);
    RUN(test_click_and_freeze);
    RUN(test_scroll);
    TEST_MAIN_END();
}
