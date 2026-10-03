#include "nmr/probe.hpp"

#include <cmath>

#include "instrument_config.hpp"

namespace nmr {

bool SampleGeometry::inside(Vec3 p) const {
    const double x_end = x_cyl_end + cone_length;
    if (p.x < x_min || p.x > x_end) return false;
    double r = radius;
    if (p.x > x_cyl_end) {
        const double u = (p.x - x_cyl_end) / cone_length;
        r = radius * (1 - u) + tip_radius * u;
    }
    return p.y * p.y + p.z * p.z <= r * r;
}

double SampleGeometry::analytic_volume() const {
    const double pi = 3.14159265358979323846;
    return pi * radius * radius * (x_cyl_end - x_min) +
           pi / 3.0 * cone_length * (radius * radius + radius * tip_radius + tip_radius * tip_radius);
}

ProbeGeometry design_probe() {
    using namespace cfg;
    ProbeGeometry pg;
    pg.mu0 = CONSTANTS_MU0_H_PER_M;
    pg.b0 = helmholtz_pair(1, B0_RADIUS_M, B0_SPACING_M, B0_TURNS_PER_COIL, B0_WINDING_WIDTH_M, B0_WINDING_DEPTH_M, 5, 5);
    const int n = static_cast<int>(RF_COIL_TURNS);
    const int per_layer = static_cast<int>(std::floor(RF_COIL_LENGTH_M / RF_COIL_WIRE_PITCH_M));  // 222
    pg.rf = layered_solenoid(0, {RF_COIL_LAYER1_RADIUS_M, RF_COIL_LAYER2_RADIUS_M}, {per_layer, n - per_layer},
                             RF_COIL_WIRE_PITCH_M, -RF_COIL_LENGTH_M / 2.0);
    return pg;
}

SampleGeometry design_sample() {
    using namespace cfg;
    return {SAMPLE_RADIUS_M, SAMPLE_X_MIN_M, SAMPLE_X_CYL_END_M, SAMPLE_CONE_LENGTH_M};
}

ProbeGeometry legacy_probe() {   // frozen 100 mm / 400-turn reference the golden maps were computed for (do not change)
    ProbeGeometry pg;
    pg.mu0 = cfg::CONSTANTS_MU0_H_PER_M;
    pg.b0 = helmholtz_pair(1, 0.2, 0.2, 312, 0.028, 0.027, 5, 5);
    pg.rf = layered_solenoid(0, {0.020225, 0.020675}, {222, 178}, 0.45e-3, -0.05);
    return pg;
}

void transverse(Vec3 b0, Vec3 b1, double& b1_perp, double& phi) {
    const Vec3 n = b0 * (1.0 / b0.norm());
    Vec3 ex = Vec3{1, 0, 0} - n * n.x;
    ex = ex * (1.0 / ex.norm());
    const Vec3 ey = n.cross(ex);
    const Vec3 bp = b1 - n * b1.dot(n);
    b1_perp = bp.norm();
    phi = std::atan2(bp.dot(ey), bp.dot(ex));
}

std::vector<Voxel> voxelize(const ProbeGeometry& pg, const SampleGeometry& sg, double g) {
    std::vector<Voxel> out;
    const double x1 = sg.x_cyl_end + sg.cone_length, r = sg.radius;
    // centred grid: points at k*g + g/2 symmetric about the sample axis; along x aligned to x_min
    const int nyz = static_cast<int>(std::ceil(r / g));
    for (double x = sg.x_min + g / 2; x < x1; x += g)
        for (int iy = -nyz; iy < nyz; ++iy)
            for (int iz = -nyz; iz < nyz; ++iz) {
                const Vec3 p{x, (iy + 0.5) * g, (iz + 0.5) * g};
                // boundary voxels carry the fraction of their volume inside the sample (4 x 4 x 4 sub-samples),
                // so the curved surface is not a staircase (whole-voxel counting is 2.4 % low at a 2.5 mm pitch)
                int in = 0;
                for (int a = 0; a < 4; ++a)
                    for (int b = 0; b < 4; ++b)
                        for (int c = 0; c < 4; ++c)
                            in += sg.inside({p.x + (a - 1.5) * g / 4, p.y + (b - 1.5) * g / 4, p.z + (c - 1.5) * g / 4});
                if (in == 0) continue;
                const Vec3 b0 = pg.b0.field_per_amp(p, pg.mu0), b1 = pg.rf.field_per_amp(p, pg.mu0);
                Voxel v;
                v.p = p;
                v.volume = g * g * g * in / 64.0;
                v.b0_per_amp = b0.norm();
                v.b0_hat = b0 * (1.0 / v.b0_per_amp);
                transverse(b0, b1, v.b1_perp, v.b1_phi);
                out.push_back(v);
            }
    return out;
}

}  // namespace nmr
