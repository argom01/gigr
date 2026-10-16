#include "board_config.h"
#include "flex_cal.h"
#include "test.h"

static void test_normalize_polarity(void)
{
    // index: voltage falls on bend
    CHECK_NEAR(flex_normalize(2300, 2300, 1300), 0.0f, 1e-6);
    CHECK_NEAR(flex_normalize(1800, 2300, 1300), 0.5f, 1e-6);
    CHECK_NEAR(flex_normalize(1300, 2300, 1300), 1.0f, 1e-6);
    // thumb: voltage rises on bend
    CHECK_NEAR(flex_normalize(1000, 1000, 2000), 0.0f, 1e-6);
    CHECK_NEAR(flex_normalize(1500, 1000, 2000), 0.5f, 1e-6);
    // clamping
    CHECK_NEAR(flex_normalize(500, 1000, 2000), 0.0f, 1e-6);
    CHECK_NEAR(flex_normalize(2500, 1000, 2000), 1.0f, 1e-6);
    CHECK_NEAR(flex_normalize(2500, 2300, 1300), 0.0f, 1e-6);
    // degenerate calibration
    CHECK_NEAR(flex_normalize(1000, 1000, 1000), 0.0f, 1e-6);
}

static void test_masks(void)
{
    const float open_mv[FINGER_COUNT] = { 1000, 2300, 2300, 2300, 2300 };
    const float closed_mv[FINGER_COUNT] = { 2000, 1300, 2250, 2600, 1300 };
    const uint8_t span = flex_cal_span_mask(open_mv, closed_mv, 150);
    CHECK(span & (1u << FINGER_THUMB));
    CHECK(span & (1u << FINGER_INDEX));
    CHECK(!(span & (1u << FINGER_MIDDLE)));     // only 50 mV
    CHECK(span & (1u << FINGER_RING));
    const uint8_t pol = flex_cal_polarity_mask(open_mv, closed_mv);
    CHECK(pol & (1u << FINGER_THUMB));          // rises, as wired
    CHECK(pol & (1u << FINGER_INDEX));          // falls, as wired
    CHECK(!(pol & (1u << FINGER_RING)));        // rises: reversed
}

static void test_resistance_model(void)
{
    // fingers: flex to VDD, fixed to GND -> higher R gives lower voltage
    const float r_lo = flex_mv_to_ohm(2000, FINGER_INDEX);
    const float r_hi = flex_mv_to_ohm(1500, FINGER_INDEX);
    CHECK(r_hi > r_lo);
    // thumb: inverted divider -> higher voltage means higher R
    CHECK(flex_mv_to_ohm(2000, FINGER_THUMB) > flex_mv_to_ohm(1500, FINGER_THUMB));
    // mid-scale equals the fixed resistor on both topologies
    CHECK_NEAR(flex_mv_to_ohm(FLEX_VDD_MV / 2, FINGER_INDEX), 47000.0f, 1.0f);
    CHECK_NEAR(flex_mv_to_ohm(FLEX_VDD_MV / 2, FINGER_THUMB), 47000.0f, 1.0f);
}

static void test_welford(void)
{
    welford_t w;
    welford_reset(&w);
    const float v[] = { 2, 4, 4, 4, 5, 5, 7, 9 };
    for (unsigned i = 0; i < sizeof(v) / sizeof(v[0]); i++) {
        welford_add(&w, v[i]);
    }
    CHECK_NEAR(w.mean, 5.0f, 1e-5);
    CHECK_NEAR(welford_std(&w), 2.13809f, 1e-4);   // sample std
}

int main(void)
{
    RUN(test_normalize_polarity);
    RUN(test_masks);
    RUN(test_resistance_model);
    RUN(test_welford);
    TEST_MAIN_END();
}
