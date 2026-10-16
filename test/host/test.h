#ifndef TEST_H
#define TEST_H

// Minimal test helpers: each check prints on failure, main returns the count.

#include <math.h>
#include <stdio.h>

static int g_failures;
static int g_checks;

#define CHECK(cond)                                                              \
    do {                                                                         \
        g_checks++;                                                              \
        if (!(cond)) {                                                           \
            g_failures++;                                                        \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
        }                                                                        \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                    \
    do {                                                                         \
        g_checks++;                                                              \
        const double a_ = (double)(a), b_ = (double)(b);                         \
        if (!(fabs(a_ - b_) <= (double)(tol))) {                                 \
            g_failures++;                                                        \
            printf("FAIL %s:%d: %s = %g, expected %g +- %g\n", __FILE__, __LINE__, \
                   #a, a_, b_, (double)(tol));                                   \
        }                                                                        \
    } while (0)

#define RUN(fn)                                                                  \
    do {                                                                         \
        printf("-- %s\n", #fn);                                                  \
        fn();                                                                    \
    } while (0)

#define TEST_MAIN_END()                                                          \
    printf("%d checks, %d failures\n", g_checks, g_failures);                    \
    return g_failures ? 1 : 0

// Deterministic pseudo random numbers (uniform -1..1)
static unsigned g_rng = 12345u;
static inline float rnd(void)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return (float)((g_rng >> 8) & 0xffff) / 32768.0f - 1.0f;
}

#endif
