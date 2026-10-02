// Minimal test harness: every check prints the model quantity, expected, actual and tolerance on failure.
#pragma once

#include <cmath>
#include <cstdio>
#include <string>

inline int g_fail = 0, g_pass = 0;

#define CHECK(cond, what)                                                                   \
    do {                                                                                    \
        if (cond) ++g_pass;                                                                 \
        else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, what); }      \
    } while (0)

// |actual - expected| <= abs_tol + rel_tol * |expected|
#define CHECK_NEAR(actual, expected, abs_tol, rel_tol, what)                                                     \
    do {                                                                                                         \
        const double a_ = (actual), e_ = (expected);                                                             \
        const double tol_ = (abs_tol) + (rel_tol) * std::fabs(e_);                                               \
        if (std::fabs(a_ - e_) <= tol_ && std::isfinite(a_)) ++g_pass;                                           \
        else { ++g_fail; std::printf("FAIL %s:%d  %s: expected %.10g, actual %.10g, tolerance %.3g\n", __FILE__, \
                                     __LINE__, what, e_, a_, tol_); }                                            \
    } while (0)

inline int finish(const char* name) {
    std::printf("%s: %d passed, %d failed\n", name, g_pass, g_fail);
    return g_fail ? 1 : 0;
}
