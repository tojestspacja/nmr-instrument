#include "nmr/field.hpp"

#include <cmath>

#include "nmr/units.hpp"

namespace nmr {

void ellipke(double m, double& K, double& E) {
    // AGM: K = pi / (2 AGM(1, sqrt(1-m))); E from the same iteration (Abramowitz & Stegun 17.6)
    double a = 1.0, b = std::sqrt(1.0 - m), c2sum = m / 2.0, pow2 = 0.5;
    for (int i = 0; i < 40; ++i) {
        const double an = 0.5 * (a + b), bn = std::sqrt(a * b), cn = 0.5 * (a - b);
        pow2 *= 2.0;
        c2sum += pow2 * cn * cn;
        a = an;
        b = bn;
        if (std::fabs(cn) < 1e-17 * a) break;
    }
    K = kPi / (2.0 * a);
    E = K * (1.0 - c2sum);
}

Vec3 loop_field(double a, double i, int axis, double axis_offset, Vec3 p, double mu0) {
    // cylindrical coordinates about the loop axis
    double ax, r1, r2;  // axial coordinate, two transverse coordinates
    if (axis == 0) { ax = p.x - axis_offset; r1 = p.y; r2 = p.z; }
    else if (axis == 1) { ax = p.y - axis_offset; r1 = p.z; r2 = p.x; }
    else { ax = p.z - axis_offset; r1 = p.x; r2 = p.y; }
    const double rho = std::hypot(r1, r2);
    const double q = (a + rho) * (a + rho) + ax * ax;
    const double m = 4.0 * a * rho / q;
    double K, E;
    ellipke(m, K, E);
    const double c = mu0 * i / (2.0 * kPi * std::sqrt(q));
    const double d = (a - rho) * (a - rho) + ax * ax;
    const double b_ax = c * (K + (a * a - rho * rho - ax * ax) / d * E);
    double b_rho = 0.0;
    if (rho > 1e-12) b_rho = c * ax / rho * (-K + (a * a + rho * rho + ax * ax) / d * E);
    const double b1 = rho > 1e-12 ? b_rho * r1 / rho : 0.0, b2 = rho > 1e-12 ? b_rho * r2 / rho : 0.0;
    if (axis == 0) return {b_ax, b1, b2};
    if (axis == 1) return {b2, b_ax, b1};
    return {b1, b2, b_ax};
}

Vec3 Coil::field_per_amp(Vec3 p, double mu0) const {
    Vec3 b;
    for (const auto& f : filaments) b = b + loop_field(f.radius, f.turns, axis, f.position, p, mu0);
    return b;
}

Coil helmholtz_pair(int axis, double radius, double spacing, double turns, double width, double depth, int nw, int nd) {
    Coil c;
    c.axis = axis;
    const double per = turns / (nw * nd);
    for (int s = -1; s <= 1; s += 2)
        for (int iw = 0; iw < nw; ++iw)
            for (int id = 0; id < nd; ++id) {
                const double pos = s * spacing / 2.0 + width * ((iw + 0.5) / nw - 0.5);
                const double r = radius + depth * ((id + 0.5) / nd - 0.5);
                c.filaments.push_back({r, pos, per});
            }
    return c;
}

Coil layered_solenoid(int axis, const std::vector<double>& radii, const std::vector<int>& turns, double pitch,
                      double x_start) {
    Coil c;
    c.axis = axis;
    for (size_t k = 0; k < radii.size(); ++k)
        for (int n = 0; n < turns[k]; ++n) c.filaments.push_back({radii[k], x_start + (n + 0.5) * pitch, 1.0});
    return c;
}

}  // namespace nmr
