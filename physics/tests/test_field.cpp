// Magnetostatics: elliptic integrals, single-loop fields, Helmholtz pair, the RF coil; plus golden values from the
// legacy Mathematica field map (Biot-Savart, independently reproduced in the audit).
#include "check.hpp"
#include "instrument_config.hpp"
#include "nmr/field.hpp"
#include "nmr/probe.hpp"

using namespace nmr;
static const double MU0 = cfg::CONSTANTS_MU0_H_PER_M;

int main() {
    {
        double K, E;
        ellipke(0, K, E);
        CHECK_NEAR(K, 1.5707963267948966, 1e-15, 0, "K(0)");
        CHECK_NEAR(E, 1.5707963267948966, 1e-15, 0, "E(0)");
        ellipke(0.5, K, E);   // A&S table 17.1
        CHECK_NEAR(K, 1.8540746773013719, 1e-14, 0, "K(0.5)");
        CHECK_NEAR(E, 1.3506438810476755, 1e-14, 0, "E(0.5)");
        ellipke(0.99, K, E);
        CHECK_NEAR(K, 3.6956373629898747, 1e-13, 0, "K(0.99)");
        CHECK_NEAR(E, 1.0159935450252240, 1e-13, 0, "E(0.99)");
    }
    // loop on its axis: B = mu0 I a^2 / (2 (a^2 + z^2)^1.5)
    {
        const double a = 0.05, I = 2.0;
        for (double zz : {0.0, 0.01, 0.07, -0.2}) {
            const Vec3 b = loop_field(a, I, 2, 0, {0, 0, zz}, MU0);
            CHECK_NEAR(b.z, MU0 * I * a * a / (2 * std::pow(a * a + zz * zz, 1.5)), 0, 1e-12, "loop on-axis field");
            CHECK_NEAR(std::hypot(b.x, b.y), 0, 1e-18, 0, "loop on-axis field has no transverse part");
        }
        // far field: dipole m = I pi a^2, B_axis = mu0 m / (2 pi r^3), B_equator = -mu0 m / (4 pi r^3)
        const double r = 5.0, m = I * 3.14159265358979323846 * a * a;
        CHECK_NEAR(loop_field(a, I, 0, 0, {r, 0, 0}, MU0).x, MU0 * m / (2 * 3.14159265358979323846 * r * r * r), 0, 1e-3, "far field on axis (dipole)");
        CHECK_NEAR(loop_field(a, I, 0, 0, {0, r, 0}, MU0).x, -MU0 * m / (4 * 3.14159265358979323846 * r * r * r), 0, 1e-3, "far field in the plane (dipole)");
        // axis permutation: the same loop about y gives the same field rotated
        const Vec3 bx = loop_field(a, I, 0, 0.01, {0.02, 0.03, 0.004}, MU0);
        const Vec3 by = loop_field(a, I, 1, 0.01, {0.004, 0.02, 0.03}, MU0);
        CHECK_NEAR(by.y, bx.x, 0, 1e-12, "loop about y: axial component");
        CHECK_NEAR(by.z, bx.y, 1e-18, 1e-12, "loop about y: transverse component 1");
        CHECK_NEAR(by.x, bx.z, 1e-18, 1e-12, "loop about y: transverse component 2");
    }
    // Helmholtz pair, thin wire: centre field and 4th-order error coefficients
    {
        const double R = cfg::B0_RADIUS_M, N = cfg::B0_TURNS_PER_COIL;
        const Coil thin = helmholtz_pair(1, R, R, N, 0, 0, 1, 1);
        CHECK_NEAR(thin.field_per_amp({0, 0, 0}, MU0).y, std::pow(0.8, 1.5) * MU0 * N / R, 0, 1e-12, "Helmholtz centre (thin wire)");
        CHECK_NEAR(cfg::B0_FIELD_PER_AMP_THIN_WIRE_T_PER_A, std::pow(0.8, 1.5) * MU0 * N / R, 0, 1e-12, "generated thin-wire value");
        const double b0 = thin.field_per_amp({0, 0, 0}, MU0).y, s = 0.010;
        const double ax = thin.field_per_amp({0, s, 0}, MU0).norm() / b0 - 1, rad = thin.field_per_amp({s, 0, 0}, MU0).norm() / b0 - 1;
        CHECK_NEAR(ax, -1.152 * std::pow(s / R, 4), 0, 0.01, "Helmholtz axial error ~ -144/125 (s/R)^4");
        CHECK_NEAR(rad, -0.432 * std::pow(s / R, 4), 0, 0.01, "Helmholtz radial error ~ -(3/8)(144/125) (s/R)^4");
    }
    // the FROZEN 100 mm reference coils against the legacy Biot-Savart map (Mathematica, fieldmap-tube.json). Uses
    // legacy_probe(), not the current design, so this stays an independent solver cross-check after the design coil
    // changes (e.g. to the 60 mm active region). See docs/design-closure.md.
    {
        const ProbeGeometry pg = legacy_probe();
        // B0 centre: 2.1034711 mT at 1.5 A (map header B0_centre_mT_at_1.5A)
        CHECK_NEAR(pg.b0.field_per_amp({0, 0, 0}, MU0).y * 1.5, 2.1034711e-3, 0, 2e-7, "B0 centre, finite-section pair (golden: map)");
        // B1 centre per amp: frozen 100 mm two-layer winding, 5.029625e-3 T/A (map centre.b1)
        CHECK_NEAR(pg.rf.field_per_amp({0, 0, 0}, MU0).x, 5.029625e-3, 0, 2e-6, "B1/I at the 100 mm reference centre (golden: map)");
        // voxel spot checks from the audit (position mm -> b1perp T/A, phi rad)
        struct Spot { double x, y, z, b1, phi; };
        const Spot spots[] = {{-56.75e-3, -12.75e-3, -5.25e-3, 1.558114e-3, -0.20769}, {5.75e-3, -0.25e-3, 4.75e-3, 4.966064e-3, -0.00833}};
        for (const auto& sp : spots) {
            const Vec3 p{sp.x, sp.y, sp.z};
            double b1p, phi;
            transverse(pg.b0.field_per_amp(p, MU0), pg.rf.field_per_amp(p, MU0), b1p, phi);
            CHECK_NEAR(b1p, sp.b1, 0, 2e-6, "B1perp/I at a map voxel (golden)");
            CHECK_NEAR(phi, sp.phi, 2e-4, 0, "phi at a map voxel (golden)");
        }
    }
    return finish("test_field");
}
