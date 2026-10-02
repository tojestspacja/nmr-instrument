// The sample as voxels in the instrument's real fields.
//
// Frame (the probe frame, as in design/instrument.yaml): x along the RF-coil axis, origin at the winding centre;
// y along the B0 pair's axis; z completes the right-handed set.
// Per voxel the model needs the static field (at a reference current) and the RF coil's field per ampere resolved in
// the transverse frame of the LOCAL B0:  x' = (coil axis) projected perpendicular to B0, y' = B0_hat x x'.
#pragma once

#include <vector>

#include "nmr/bloch.hpp"
#include "nmr/field.hpp"

namespace nmr {

struct Voxel {
    Vec3 p;              // position [m]
    double volume;       // [m^3]
    double b0_per_amp;   // |B0| per ampere of B0 current at this point [T/A] (B0 is linear in I without Earth field)
    Vec3 b0_hat;         // direction of B0 (fixed for I > 0)
    double b1_perp;      // |B1 perpendicular to local B0| per ampere of RF-coil current [T/A]
    double b1_phi;       // direction of that component in the local transverse frame [rad]
};

struct SampleGeometry {   // cylinder along x from x_min to x_cyl_end (radius r), then a cone over cone_length
    double radius, x_min, x_cyl_end, cone_length, tip_radius = 0.002;
    bool inside(Vec3 p) const;
    double analytic_volume() const;
};

struct ProbeGeometry {
    Coil b0;    // B0 pair, per amp
    Coil rf;    // RF coil, per amp
    double mu0;
};

// Probe geometry from generated/instrument_config.hpp (the canonical design).
ProbeGeometry design_probe();
SampleGeometry design_sample();

// Centred grid of pitch g over the sample; fields computed by Biot-Savart for every voxel.
std::vector<Voxel> voxelize(const ProbeGeometry& pg, const SampleGeometry& sg, double g);

// Transverse decomposition used for every voxel (exposed for tests): returns |B1perp| and phi.
void transverse(Vec3 b0, Vec3 b1, double& b1_perp, double& phi);

}  // namespace nmr
