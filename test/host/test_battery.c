#include "battery.h"
#include "board_config.h"
#include "test.h"

static void test_lut(void)
{
    CHECK(battery_pct_from_mv(3000) == 0);
    CHECK(battery_pct_from_mv(4300) == 100);
    CHECK(battery_pct_from_mv(3800) == 50);
    CHECK(battery_pct_from_mv(3775) == 45);
    uint8_t prev = 0;
    for (int mv = 3300; mv <= 4200; mv += 10) {
        const uint8_t p = battery_pct_from_mv((float)mv);
        CHECK(p >= prev);
        prev = p;
    }
}

static void test_hysteresis(void)
{
    battery_est_t b = { 0 };
    CHECK(battery_est_update(&b, 3800, false, 1.0f) == 50);
    // small upward wobble while discharging must not raise the shown value
    for (int i = 0; i < 120; i++) {
        battery_est_update(&b, 3805, false, 1.0f);
    }
    CHECK(b.pct_shown == 50);
    // a clear discharge is followed
    for (int i = 0; i < 300; i++) {
        battery_est_update(&b, 3700, false, 1.0f);
    }
    CHECK(b.pct_shown < 35);
}

static void test_charging_offset(void)
{
    battery_est_t b = { 0 };
    const uint8_t p = battery_est_update(&b, 3800 + BAT_CHARGE_OFFSET_MV, true, 1.0f);
    CHECK(p == 50);
}

int main(void)
{
    RUN(test_lut);
    RUN(test_hysteresis);
    RUN(test_charging_offset);
    TEST_MAIN_END();
}
