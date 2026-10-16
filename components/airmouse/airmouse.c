#include "airmouse.h"

#include <math.h>
#include <string.h>
#include "board_config.h"
#include "vec3.h"

void airmouse_params_default(airmouse_params_t *p)
{
    memset(p, 0, sizeof(*p));
    p->enter_th = AM_ENTER_TH;
    p->exit_th = AM_EXIT_TH;
    p->pose_ms = AM_POSE_MS;
    p->settle_ms = AM_SETTLE_MS;
    p->x_source = 0;
    p->gain_min = AM_GAIN_MIN;
    p->gain_max = AM_GAIN_MAX;
    p->v1_dps = AM_V1_DPS;
    p->v2_dps = AM_V2_DPS;
    p->mincutoff_hz = AM_MINCUTOFF_HZ;
    p->beta = AM_BETA;
    p->dcutoff_hz = AM_DCUTOFF_HZ;
    p->deadzone_dps = AM_DEADZONE_DPS;
    p->press_th = AM_CLICK_PRESS_TH;
    p->release_th = AM_CLICK_RELEASE_TH;
    p->dominance = AM_CLICK_DOMINANCE;
    p->debounce_ms = AM_CLICK_DEBOUNCE_MS;
    p->freeze_ms = AM_CLICK_FREEZE_MS;
    p->click_vel_th = AM_CLICK_VEL_TH;
    p->scroll_hold_ms = AM_SCROLL_HOLD_MS;
    p->scroll_gain = AM_SCROLL_GAIN;
}

void airmouse_init(airmouse_t *am, const airmouse_params_t *p)
{
    memset(am, 0, sizeof(*am));
    airmouse_set_params(am, p);
    airmouse_reset(am);
}

void airmouse_set_params(airmouse_t *am, const airmouse_params_t *p)
{
    am->p = *p;
    am->fx.mincutoff = am->fy.mincutoff = p->mincutoff_hz;
    am->fx.beta = am->fy.beta = p->beta;
    am->fx.dcutoff = am->fy.dcutoff = p->dcutoff_hz;
}

static void reset_motion(airmouse_t *am)
{
    one_euro_init(&am->fx, am->p.mincutoff_hz, am->p.beta, am->p.dcutoff_hz);
    one_euro_init(&am->fy, am->p.mincutoff_hz, am->p.beta, am->p.dcutoff_hz);
    am->res_x = am->res_y = am->res_wheel = am->res_pan = 0.0f;
}

void airmouse_reset(airmouse_t *am)
{
    am->active = false;
    am->pose_ms = 0.0f;
    am->settle_left_ms = 0.0f;
    am->freeze_left_ms = 0.0f;
    am->blend = 1.0f;
    memset(&am->left, 0, sizeof(am->left));
    memset(&am->right, 0, sizeof(am->right));
    am->scrolling = false;
    am->both_ms = 0.0f;
    am->prev_init = false;
    reset_motion(am);
}

static float smoothstep01(float t)
{
    if (t <= 0.0f) {
        return 0.0f;
    }
    if (t >= 1.0f) {
        return 1.0f;
    }
    return t * t * (3.0f - 2.0f * t);
}

static int32_t take_int(float *acc)
{
    const int32_t v = (int32_t)*acc;
    *acc -= (float)v;
    return v;
}

void airmouse_screen_rates(const airmouse_params_t *p, const float gyro_dps[3], const float up[3], float *x_rate,
                           float *y_rate)
{
    // forearm axis: body +X
    const float fwd[3] = { 1.0f, 0.0f, 0.0f };
    float lat[3];
    v3_cross(up, fwd, lat);             // horizontal axis across the hand (+Y when level)
    if (v3_norm(lat) < 0.3f) {
        // fingers pointing almost straight up/down: fall back to body Y
        // made orthogonal to up
        const float y[3] = { 0.0f, 1.0f, 0.0f };
        const float d = v3_dot(y, up);
        lat[0] = y[0] - d * up[0];
        lat[1] = y[1] - d * up[1];
        lat[2] = y[2] - d * up[2];
    }
    v3_normalize(lat);

    const float yaw = v3_dot(gyro_dps, up);     // + = turning left (CCW from above)
    const float pitch = v3_dot(gyro_dps, lat);  // + = fingertips going down

    float x = p->x_source == 1 ? gyro_dps[0] : -yaw;
    float y = pitch;
    if (p->invert_x) {
        x = -x;
    }
    if (p->invert_y) {
        y = -y;
    }
    *x_rate = x;
    *y_rate = y;
}

// Hysteresis + debounce for one button. `want` = press condition, `drop` =
// release condition.
static void button_update(am_button_t *b, bool want, bool drop, float dt_ms, int debounce_ms)
{
    const bool change = b->pressed ? drop : want;
    if (change) {
        b->cond_ms += dt_ms;
        if (b->cond_ms >= (float)debounce_ms) {
            b->pressed = !b->pressed;
            b->cond_ms = 0.0f;
        }
    } else {
        b->cond_ms = 0.0f;
    }
}

void airmouse_step(airmouse_t *am, bool enabled, const float flex[FINGER_COUNT], const float gyro_dps[3],
                   const float up[3], float dt, airmouse_out_t *out)
{
    const airmouse_params_t *p = &am->p;
    const float dt_ms = dt * 1000.0f;
    memset(out, 0, sizeof(*out));

    // ── pose: ring + pinky curled ──
    const float ring = flex[FINGER_RING], pinky = flex[FINGER_PINKY];
    if (!enabled) {
        if (am->active) {
            out->exited = true;
        }
        airmouse_reset(am);
        return;
    }
    if (!am->active) {
        am->pose_ms = (ring > p->enter_th && pinky > p->enter_th) ? am->pose_ms + dt_ms : 0.0f;
        if (am->pose_ms >= (float)p->pose_ms) {
            airmouse_reset(am);
            am->active = true;
            am->settle_left_ms = (float)p->settle_ms;
            out->entered = true;
        } else {
            return;
        }
    } else {
        am->pose_ms = (ring < p->exit_th || pinky < p->exit_th) ? am->pose_ms + dt_ms : 0.0f;
        if (am->pose_ms >= (float)p->pose_ms) {
            airmouse_reset(am);
            out->exited = true;
            return;
        }
    }
    out->active = true;

    // ── click fingers ──
    const float index = flex[FINGER_INDEX], middle = flex[FINGER_MIDDLE];
    if (am->prev_init && dt > 0.0f) {
        const float k = dt / (0.02f + dt);      // ~8 Hz smoothing of the bend velocity
        am->vel_index += k * ((index - am->prev_index) / dt - am->vel_index);
        am->vel_middle += k * ((middle - am->prev_middle) / dt - am->vel_middle);
    }
    am->prev_index = index;
    am->prev_middle = middle;
    am->prev_init = true;

    // both fingers bent together (no dominance) for a while = scroll mode
    const bool both = index > p->press_th && middle > p->press_th && fabsf(index - middle) < p->dominance;
    if (!am->scrolling) {
        am->both_ms = both ? am->both_ms + dt_ms : 0.0f;
        if (am->both_ms >= (float)p->scroll_hold_ms && !am->left.pressed && !am->right.pressed) {
            am->scrolling = true;
            am->res_wheel = am->res_pan = 0.0f;
        }
    } else if (index < p->release_th || middle < p->release_th) {
        am->scrolling = false;
        am->both_ms = 0.0f;
    }

    if (!am->scrolling) {
        button_update(&am->left, index > p->press_th && index - middle > p->dominance, index < p->release_th, dt_ms,
                      p->debounce_ms);
        button_update(&am->right, middle > p->press_th && middle - index > p->dominance, middle < p->release_th,
                      dt_ms, p->debounce_ms);
    }
    out->buttons = (am->left.pressed ? 0x01 : 0) | (am->right.pressed ? 0x02 : 0);
    out->scrolling = am->scrolling;

    // a fast bend or release of a click finger jerks the hand: freeze the cursor
    if (fabsf(am->vel_index) > p->click_vel_th || fabsf(am->vel_middle) > p->click_vel_th) {
        am->freeze_left_ms = (float)p->freeze_ms;
    }

    // ── motion ──
    float xr, yr;
    airmouse_screen_rates(p, gyro_dps, up, &xr, &yr);
    xr = one_euro_filter(&am->fx, xr, dt);
    yr = one_euro_filter(&am->fy, yr, dt);

    // soft deadzone on the 2-D rate
    const float mag = sqrtf(xr * xr + yr * yr);
    const float dz = p->deadzone_dps;
    float scale = dz > 0.0f ? (mag - dz) / dz : 1.0f;
    scale = scale < 0.0f ? 0.0f : (scale > 1.0f ? 1.0f : scale);
    xr *= scale;
    yr *= scale;
    const float speed = mag * scale;
    out->speed_dps = speed;

    if (am->settle_left_ms > 0.0f) {
        am->settle_left_ms -= dt_ms;
        out->settling = true;
        return;
    }
    if (am->freeze_left_ms > 0.0f) {
        am->freeze_left_ms -= dt_ms;
        am->blend = 0.0f;
        am->res_x = am->res_y = 0.0f;
        out->frozen = true;
        return;
    }
    if (am->blend < 1.0f) {
        am->blend += dt_ms / 100.0f;    // ease back in over 100 ms
        if (am->blend > 1.0f) {
            am->blend = 1.0f;
        }
    }

    if (am->scrolling) {
        // fingertips down -> scroll down (wheel negative); right -> pan right
        am->res_wheel -= yr * p->scroll_gain * dt;
        am->res_pan += xr * p->scroll_gain * dt;
        out->wheel = take_int(&am->res_wheel);
        out->pan = take_int(&am->res_pan);
        return;
    }

    const float k = smoothstep01((speed - p->v1_dps) / (p->v2_dps - p->v1_dps));
    const float gain = p->gain_min + (p->gain_max - p->gain_min) * k;
    out->gain = gain;
    am->res_x += gain * xr * dt * am->blend;
    am->res_y += gain * yr * dt * am->blend;
    out->dx = take_int(&am->res_x);
    out->dy = take_int(&am->res_y);
}
