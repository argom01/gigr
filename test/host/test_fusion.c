#include "gyro_bias.h"
#include "mahony.h"
#include "one_euro.h"
#include "quat.h"
#include "test.h"
#include "vec3.h"

static void test_quat_basics(void)
{
    // 90 deg about Z: X -> Y
    const float s = sqrtf(0.5f);
    const float q[4] = { s, 0, 0, s };
    const float x[3] = { 1, 0, 0 };
    float r[3];
    quat_rotate(q, x, r);
    CHECK_NEAR(r[0], 0, 1e-5);
    CHECK_NEAR(r[1], 1, 1e-5);
    CHECK_NEAR(r[2], 0, 1e-5);

    float roll, pitch, yaw;
    quat_to_euler_deg(q, &roll, &pitch, &yaw);
    CHECK_NEAR(yaw, 90, 1e-3);
    CHECK_NEAR(roll, 0, 1e-3);
    CHECK_NEAR(pitch, 0, 1e-3);
}

static void test_from_two_vectors(void)
{
    float a[3] = { 0.3f, -0.5f, 0.8f };
    v3_normalize(a);
    const float up[3] = { 0, 0, 1 };
    float q[4], r[3];
    quat_from_two_vectors(a, up, q);
    quat_rotate(q, a, r);
    CHECK_NEAR(r[0], 0, 1e-5);
    CHECK_NEAR(r[1], 0, 1e-5);
    CHECK_NEAR(r[2], 1, 1e-5);

    // opposite vectors
    const float down[3] = { 0, 0, -1 };
    quat_from_two_vectors(down, up, q);
    quat_rotate(q, down, r);
    CHECK_NEAR(r[2], 1, 1e-5);
}

static void test_up_in_body(void)
{
    // body rolled +30 deg about X: world up seen in body frame
    const float h = DEG2RAD(30.0f) / 2.0f;
    const float q[4] = { cosf(h), sinf(h), 0, 0 };
    float up[3], ref[3];
    quat_up_in_body(q, up);
    float qc[4];
    quat_conj(q, qc);
    const float z[3] = { 0, 0, 1 };
    quat_rotate(qc, z, ref);
    for (int i = 0; i < 3; i++) {
        CHECK_NEAR(up[i], ref[i], 1e-5);
    }
}

static void test_mahony_static_tilt(void)
{
    // accel of a body rolled by 20 deg: gravity reaction in body frame
    const float roll = DEG2RAD(20.0f);
    const float acc[3] = { 0.0f, sinf(roll), cosf(roll) };
    const float gyro[3] = { 0, 0, 0 };
    mahony_t f;
    mahony_init(&f, 1.0f, 0.01f);
    for (int i = 0; i < 2000; i++) {
        mahony_update(&f, gyro, acc, 0.005f);
    }
    float r, p, y;
    quat_to_euler_deg(f.q, &r, &p, &y);
    CHECK_NEAR(r, 20.0f, 0.5f);
    CHECK_NEAR(p, 0.0f, 0.5f);
}

static void test_mahony_yaw_rate(void)
{
    const float acc[3] = { 0, 0, 1 };
    const float gyro[3] = { 0, 0, DEG2RAD(90.0f) };
    mahony_t f;
    mahony_init(&f, 1.0f, 0.0f);
    mahony_update(&f, gyro, acc, 0.005f);   // initialises from accel
    for (int i = 0; i < 200; i++) {         // 1 s
        mahony_update(&f, gyro, acc, 0.005f);
    }
    float r, p, y;
    quat_to_euler_deg(f.q, &r, &p, &y);
    CHECK_NEAR(y, 90.0f, 1.0f);
}

static void test_gyro_bias_tracking(void)
{
    gyro_bias_t b;
    gyro_bias_init(&b, 1.5f, 0.05f, 1.0f, 2.0f);
    const float acc[3] = { 0, 0, 1 };
    for (int i = 0; i < 200 * 30; i++) {
        const float g[3] = { 0.5f + 0.05f * rnd(), -0.3f + 0.05f * rnd(), 0.1f };
        gyro_bias_update(&b, g, acc, 0.005f);
    }
    CHECK(b.still);
    CHECK_NEAR(b.bias[0], 0.5f, 0.05f);
    CHECK_NEAR(b.bias[1], -0.3f, 0.05f);
    CHECK_NEAR(b.bias[2], 0.1f, 0.05f);

    // motion: not still, bias frozen
    const float before = b.bias[0];
    for (int i = 0; i < 400; i++) {
        const float g[3] = { 50.0f, 0, 0 };
        gyro_bias_update(&b, g, acc, 0.005f);
    }
    CHECK(!b.still);
    CHECK_NEAR(b.bias[0], before, 1e-6);
}

static void test_gyro_bias_capture(void)
{
    gyro_bias_t b;
    gyro_bias_init(&b, 1.5f, 0.05f, 1.0f, 0.0f);
    const float acc[3] = { 0, 0, 1 };
    gyro_bias_capture_start(&b, 100);
    int done = 0;
    for (int i = 0; i < 1000 && !done; i++) {
        const float g[3] = { 2.0f, -1.0f, 0.5f };   // larger than the normal still gate
        gyro_bias_update(&b, g, acc, 0.005f);
        done = gyro_bias_capture_poll(&b);
    }
    CHECK(done);
    CHECK_NEAR(b.bias[0], 2.0f, 1e-4);
    CHECK_NEAR(b.bias[1], -1.0f, 1e-4);
}

static void test_one_euro(void)
{
    one_euro_t f;
    one_euro_init(&f, 1.0f, 0.02f, 1.0f);
    // noisy constant: output variance well below input variance
    double sum_in = 0, sum_out = 0;
    for (int i = 0; i < 2000; i++) {
        const float x = 10.0f + rnd();
        const float y = one_euro_filter(&f, x, 0.005f);
        if (i > 500) {
            sum_in += (x - 10.0) * (x - 10.0);
            sum_out += (y - 10.0) * (y - 10.0);
        }
    }
    CHECK(sum_out < 0.1 * sum_in);

    // fast step settles quickly thanks to beta
    one_euro_init(&f, 1.0f, 0.5f, 1.0f);
    float y = 0;
    for (int i = 0; i < 40; i++) {      // 200 ms
        y = one_euro_filter(&f, i == 0 ? 0.0f : 100.0f, 0.005f);
    }
    CHECK(y > 90.0f);
}

int main(void)
{
    RUN(test_quat_basics);
    RUN(test_from_two_vectors);
    RUN(test_up_in_body);
    RUN(test_mahony_static_tilt);
    RUN(test_mahony_yaw_rate);
    RUN(test_gyro_bias_tracking);
    RUN(test_gyro_bias_capture);
    RUN(test_one_euro);
    TEST_MAIN_END();
}
