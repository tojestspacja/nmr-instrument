// Bloch dynamics of one isochromat, analytic (no time stepping).
//
// Conventions (validated against an RK4 integration of dM/dt = gamma M x B in tests/test_bloch.cpp):
//   - gamma > 0 (1H). In the lab, M precesses clockwise seen from +z: dM/dt = gamma M x B.
//   - Rotating frame at the transmitter frequency f_tx: the residual field along z is dB = (f_L - f_tx)/gamma_bar,
//     i.e. an offset df = f_L - f_tx in Hz. The RF field B1 lies in the transverse plane at angle phi from x'.
//   - Effective field B_eff = (B1 cos phi, B1 sin phi, df/gamma_bar). M rotates about -B_eff (left-handed for gamma>0)
//     at |omega_eff| = 2 pi gamma_bar |B_eff|.
//   - Free precession: M_xy(t) = M_xy(0) exp(-i 2 pi df t) exp(-t/T2); M_z(t) = M0 + (M_z(0) - M0) exp(-t/T1).
// Units: SI (Hz, s, T); M is in units of M0 (dimensionless) unless stated.
#pragma once

#include <cmath>

namespace nmr {

struct Vec3 {
    double x = 0, y = 0, z = 0;
    constexpr Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(double k) const { return {x * k, y * k, z * k}; }
    constexpr double dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3 cross(Vec3 o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    double norm() const { return std::sqrt(dot(*this)); }
};

// Rotate v about the unit axis n by angle theta (right-handed, Rodrigues).
inline Vec3 rotate(Vec3 v, Vec3 n, double theta) {
    const double c = std::cos(theta), s = std::sin(theta);
    return v * c + n.cross(v) * s + n * (n.dot(v) * (1.0 - c));
}

// A hard pulse in the frame rotating at f_tx: rotation about -B_eff for duration t.
//   b1      : rotating-frame RF amplitude [T]
//   phase   : RF phase phi [rad] (0 = along +x')
//   offset  : f_L - f_tx [Hz]
inline Vec3 pulse(Vec3 m, double gamma_bar, double b1, double phase, double offset_hz, double t) {
    const double bx = b1 * std::cos(phase), by = b1 * std::sin(phase), bz = offset_hz / gamma_bar;
    const double b = std::sqrt(bx * bx + by * by + bz * bz);
    if (b == 0.0) return m;
    const Vec3 n{-bx / b, -by / b, -bz / b};
    return rotate(m, n, 2.0 * 3.14159265358979323846 * gamma_bar * b * t);
}

// Free precession with relaxation for time t (exact). m0 is the equilibrium Mz (1 for normalised M).
inline Vec3 precess(Vec3 m, double offset_hz, double t, double t1, double t2, double m0 = 1.0) {
    const double ph = -2.0 * 3.14159265358979323846 * offset_hz * t;
    const double c = std::cos(ph), s = std::sin(ph);
    const double e2 = t2 > 0 ? std::exp(-t / t2) : 1.0;
    const double e1 = t1 > 0 ? std::exp(-t / t1) : 1.0;
    return {(m.x * c - m.y * s) * e2, (m.x * s + m.y * c) * e2, m0 + (m.z - m0) * e1};
}

// Curie-law equilibrium magnetization M0 = N gamma^2 hbar^2 B0 / (4 k T) for spin-1/2 [A/m].
inline double curie_magnetization(double n_per_m3, double gamma_bar, double b0, double temperature,
                                  double hbar, double k_b) {
    const double g = 2.0 * 3.14159265358979323846 * gamma_bar;
    return n_per_m3 * g * g * hbar * hbar * b0 / (4.0 * k_b * temperature);
}

}  // namespace nmr
