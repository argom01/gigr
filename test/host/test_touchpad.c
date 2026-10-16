#include "test.h"
#include "touchpad.h"

static const touchpad_cfg_t CFG = {
    .gain = 2.0f, .tap_max_ms = 180, .tap_max_move_px = 10, .scroll_div = 10.0f,
    .exit_corner_px = 60, .exit_hold_ms = 1000,
};

#define MS(x) ((int64_t)(x) * 1000)

static void feed1(touchpad_t *tp, uint16_t x, uint16_t y, int64_t t, touchpad_out_t *o)
{
    touchpad_process(tp, 1, &x, &y, t, o);
}

static void test_tap(void)
{
    touchpad_t tp;
    touchpad_out_t o;
    touchpad_init(&tp, &CFG);
    feed1(&tp, 200, 200, MS(0), &o);
    feed1(&tp, 202, 201, MS(50), &o);
    touchpad_process(&tp, 0, NULL, NULL, MS(100), &o);
    CHECK(o.click == 0x01);

    // too long -> no click
    feed1(&tp, 200, 200, MS(1000), &o);
    touchpad_process(&tp, 0, NULL, NULL, MS(1400), &o);
    CHECK(o.click == 0);
}

static void test_two_finger_tap(void)
{
    touchpad_t tp;
    touchpad_out_t o;
    touchpad_init(&tp, &CFG);
    const uint16_t x[2] = { 200, 260 }, y[2] = { 150, 150 };
    touchpad_process(&tp, 2, x, y, MS(0), &o);
    touchpad_process(&tp, 2, x, y, MS(40), &o);
    touchpad_process(&tp, 1, x, y, MS(80), &o);     // one finger lifts first
    touchpad_process(&tp, 0, NULL, NULL, MS(100), &o);
    CHECK(o.click == 0x02);
}

static void test_drag(void)
{
    touchpad_t tp;
    touchpad_out_t o;
    touchpad_init(&tp, &CFG);
    int32_t sx = 0, sy = 0;
    for (int i = 0; i <= 50; i++) {
        feed1(&tp, (uint16_t)(100 + i), (uint16_t)(100 + i / 2), MS(10 * i), &o);
        sx += o.dx;
        sy += o.dy;
    }
    CHECK(sx == 100);       // 50 px * gain 2
    CHECK(sy == 50);
    touchpad_process(&tp, 0, NULL, NULL, MS(600), &o);
    CHECK(o.click == 0);    // moved: not a tap
}

static void test_scroll(void)
{
    touchpad_t tp;
    touchpad_out_t o;
    touchpad_init(&tp, &CFG);
    int32_t wheel = 0, dx = 0;
    for (int i = 0; i <= 40; i++) {
        const uint16_t x[2] = { 200, 260 };
        const uint16_t y[2] = { (uint16_t)(100 + i), (uint16_t)(100 + i) };
        touchpad_process(&tp, 2, x, y, MS(10 * i), &o);
        wheel += o.wheel;
        dx += o.dx;
    }
    CHECK(wheel == 4);      // 40 px / 10 px per step, fingers down -> wheel up
    CHECK(dx == 0);         // no pointer motion while scrolling
}

static void test_exit_corner(void)
{
    touchpad_t tp;
    touchpad_out_t o;
    touchpad_init(&tp, &CFG);
    bool exited = false;
    for (int t = 0; t <= 1200; t += 20) {
        feed1(&tp, 20, 20, MS(t), &o);
        exited |= o.exit;
        CHECK(o.dx == 0 && o.dy == 0);
    }
    CHECK(exited);
    touchpad_process(&tp, 0, NULL, NULL, MS(1250), &o);
    CHECK(o.click == 0);
}

int main(void)
{
    RUN(test_tap);
    RUN(test_two_finger_tap);
    RUN(test_drag);
    RUN(test_scroll);
    RUN(test_exit_corner);
    TEST_MAIN_END();
}
