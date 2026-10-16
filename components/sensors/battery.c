#include "battery.h"

#include "board_config.h"

typedef struct {
    float mv;
    float pct;
} ocv_point_t;

static const ocv_point_t OCV[] = {
    { 3300, 0 },  { 3450, 5 },  { 3600, 10 }, { 3680, 20 }, { 3710, 30 }, { 3750, 40 },
    { 3800, 50 }, { 3850, 60 }, { 3920, 70 }, { 4000, 80 }, { 4100, 90 }, { 4200, 100 },
};
#define OCV_N (sizeof(OCV) / sizeof(OCV[0]))

#define FILTER_TAU_S 10.0f
#define HYSTERESIS_PCT 2

uint8_t battery_pct_from_mv(float mv)
{
    if (mv <= OCV[0].mv) {
        return 0;
    }
    if (mv >= OCV[OCV_N - 1].mv) {
        return 100;
    }
    for (unsigned i = 1; i < OCV_N; i++) {
        if (mv < OCV[i].mv) {
            const float t = (mv - OCV[i - 1].mv) / (OCV[i].mv - OCV[i - 1].mv);
            return (uint8_t)(OCV[i - 1].pct + t * (OCV[i].pct - OCV[i - 1].pct) + 0.5f);
        }
    }
    return 100;
}

uint8_t battery_est_update(battery_est_t *b, float mv, bool charging, float dt)
{
    if (charging) {
        mv -= BAT_CHARGE_OFFSET_MV;
    }
    if (!b->init) {
        b->init = true;
        b->mv_filt = mv;
        b->pct_shown = battery_pct_from_mv(mv);
        return (uint8_t)b->pct_shown;
    }
    const float k = dt / (FILTER_TAU_S + dt);
    b->mv_filt += k * (mv - b->mv_filt);

    const int pct = battery_pct_from_mv(b->mv_filt);
    const int diff = pct - b->pct_shown;
    // Follow small changes only in the plausible direction, big jumps always
    if (diff >= HYSTERESIS_PCT || diff <= -HYSTERESIS_PCT || (charging && diff > 0) || (!charging && diff < 0)) {
        b->pct_shown = pct;
    }
    return (uint8_t)b->pct_shown;
}
