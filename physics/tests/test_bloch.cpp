// Bloch dynamics against analytic results and an independent RK4 integration of the Bloch equation.
#include <random>

#include "check.hpp"
#include "instrument_config.hpp"
#include "nmr/bloch.hpp"

using namespace nmr;
static const double G = cfg::NUCLEUS_GAMMA_BAR_HZ_PER_T;   // 1H, from the canonical config
static const double PI = 3.14159265358979323846;

// Independent reference: dM/dt = 2 pi G M x B_eff, B_eff = (B1 cos phi, B1 sin phi, df / G), RK4.
static Vec3 rk4(Vec3 m, double b1, double phi, double df, double t, int steps) {
    const Vec3 b{b1 * std::cos(phi), b1 * std::sin(phi), df / G};
    const auto f = [&](Vec3 x) { return x.cross(b) * (2 * PI * G); };
    const double h = t / steps;
    for (int i = 0; i < steps; ++i) {
        const Vec3 k1 = f(m), k2 = f(m + k1 * (h / 2)), k3 = f(m + k2 * (h / 2)), k4 = f(m + k3 * h);
        m = m + (k1 + k2 * 2 + k3 * 2 + k4) * (h / 6);
    }
    return m;
}

int main() {
    const Vec3 z{0, 0, 1};
    // equilibrium is a fixed point of free precession with relaxation
    {
        const Vec3 m = precess(z, 123.0, 0.37, 2.5, 2.0);
        CHECK_NEAR(m.x, 0, 1e-15, 0, "equilibrium Mx"); CHECK_NEAR(m.y, 0, 1e-15, 0, "equilibrium My");
        CHECK_NEAR(m.z, 1, 1e-15, 0, "equilibrium Mz");
    }
    // ideal on-resonance 90 and 180 degree pulses (phi = 0): rotation about -x' takes +z to +y', then to -z
    {
        const double b1 = 1e-5, t90 = 1.0 / (4 * G * b1);
        const Vec3 m90 = pulse(z, G, b1, 0, 0, t90), m180 = pulse(z, G, b1, 0, 0, 2 * t90);
        CHECK_NEAR(m90.x, 0, 1e-12, 0, "90x: Mx"); CHECK_NEAR(m90.y, 1, 1e-12, 0, "90x: My");
        CHECK_NEAR(m90.z, 0, 1e-12, 0, "90x: Mz");
        CHECK_NEAR(m180.z, -1, 1e-12, 0, "180x: Mz");
        // phase 90 degrees (y'): +z goes to -x'
        const Vec3 m90y = pulse(z, G, b1, PI / 2, 0, t90);
        CHECK_NEAR(m90y.x, -1, 1e-12, 0, "90y: Mx");
    }
    // off-resonance pulses: analytic rotation vs RK4 (handedness and effective field)
    {
        const double cases[][4] = {{14.07e-6, 0.0, 160.0, cfg::TX_T90_S}, {14.07e-6, 0.4, -300.0, cfg::TX_T90_S}, {5e-6, 1.3, 1000, 2e-3}};
        for (const auto& c : cases) {
            const Vec3 a = pulse(z, G, c[0], c[1], c[2], c[3]), r = rk4(z, c[0], c[1], c[2], c[3], 20000);
            CHECK_NEAR(a.x, r.x, 1e-9, 0, "off-res pulse vs RK4: Mx");
            CHECK_NEAR(a.y, r.y, 1e-9, 0, "off-res pulse vs RK4: My");
            CHECK_NEAR(a.z, r.z, 1e-9, 0, "off-res pulse vs RK4: Mz");
        }
    }
    // free precession sense and the I/Q convention of the model: M_xy(t) = M_xy(0) e^{-i 2 pi df t}
    {
        const Vec3 m0{1, 0, 0};
        const double df = 100, t = 1e-3;
        const Vec3 m = precess(m0, df, t, 0, 0);
        const Vec3 r = rk4(m0, 0, 0, df, t, 20000);
        CHECK_NEAR(m.x, std::cos(2 * PI * df * t), 1e-12, 0, "precession: Mx = cos(2 pi df t)");
        CHECK_NEAR(m.y, -std::sin(2 * PI * df * t), 1e-12, 0, "precession: My = -sin(2 pi df t) (clockwise for gamma>0)");
        CHECK_NEAR(m.y, r.y, 1e-9, 0, "precession vs RK4");
    }
    // pure T1 recovery and pure T2 decay
    {
        const double t1 = 2.5, t2 = 2.0, t = 0.7;
        const Vec3 a = precess(Vec3{0, 0, 0}, 0, t, t1, t2);
        CHECK_NEAR(a.z, 1 - std::exp(-t / t1), 1e-15, 0, "T1 recovery from saturation");
        const Vec3 b = precess(Vec3{1, 0, 0}, 0, t, t1, t2);
        CHECK_NEAR(b.x, std::exp(-t / t2), 1e-15, 0, "T2 decay");
        const Vec3 c = precess(Vec3{0, 0, -1}, 0, t, t1, t2);
        CHECK_NEAR(c.z, 1 - 2 * std::exp(-t / t1), 1e-15, 0, "inversion recovery Mz(t)");
    }
    // property: pure rotations preserve |M| (random fields, phases, offsets, times)
    {
        std::mt19937_64 rng(7);
        std::uniform_real_distribution<double> u(-1, 1);
        double worst = 0;
        for (int i = 0; i < 2000; ++i) {
            Vec3 m{u(rng), u(rng), u(rng)};
            const double n0 = m.norm();
            m = pulse(m, G, 1e-5 * (1 + u(rng)), PI * u(rng), 500 * u(rng), 1e-3 * (1 + u(rng)));
            m = precess(m, 300 * u(rng), 0.01 * (1 + u(rng)), 0, 0);
            worst = std::max(worst, std::fabs(m.norm() - n0));
        }
        CHECK_NEAR(worst, 0, 1e-13, 0, "norm preserved by pulse + precession without relaxation");
    }
    // Hahn echo with ideal pulses refocuses an offset distribution exactly (no T2)
    {
        const double b1 = 1e-3, t90 = 1.0 / (4 * G * b1), tau = 0.02;   // hard: B1 >> offsets
        double sx = 0, sy = 0;
        const int n = 401;
        for (int k = 0; k < n; ++k) {
            const double df = -150 + 300.0 * k / (n - 1);
            Vec3 m = pulse(z, G, b1, 0, df, t90);
            m = precess(m, df, tau, 0, 0);
            m = pulse(m, G, b1, PI / 2, df, 2 * t90);
            m = precess(m, df, tau, 0, 0);
            sx += m.x; sy += m.y;
        }
        CHECK_NEAR(std::hypot(sx, sy) / n, 1.0, 2e-3, 0, "Hahn echo magnitude with hard pulses (finite-pulse error < 2e-3)");
    }
    // Curie law against the closed form with the canonical constants
    {
        const double b = 2.1e-3, t = 293, n = 6.687e28;
        const double gam = 2 * PI * G;
        const double expect = n * gam * gam * cfg::CONSTANTS_HBAR_J_S * cfg::CONSTANTS_HBAR_J_S * b / (4 * cfg::CONSTANTS_K_BOLTZMANN_J_PER_K * t);
        CHECK_NEAR(curie_magnetization(n, G, b, t, cfg::CONSTANTS_HBAR_J_S, cfg::CONSTANTS_K_BOLTZMANN_J_PER_K), expect, 0, 1e-14,
                   "Curie magnetization");
    }
    return finish("test_bloch");
}
