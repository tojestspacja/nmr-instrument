// Magnetostatics for the instrument's coils: exact circular-loop fields (complete elliptic integrals), built up
// into a finite-section Helmholtz pair (B0) and a two-layer solenoid (the RF coil, B1 per ampere).
// Ported from the legacy Mathematica kernel (legacy/simulator/build/nmr_exact.wls), whose maps the tests reproduce.
// Units: SI (m, A, T).
#pragma once

#include <vector>

#include "nmr/bloch.hpp"

namespace nmr {

// Complete elliptic integrals K(m), E(m) with parameter m = k^2, by the arithmetic-geometric mean (|err| < 1e-15).
void ellipke(double m, double& K, double& E);

// Field of a single circular loop of radius a [m] carrying current i [A], centred at the origin, axis along +axis
// (0 = x, 1 = y, 2 = z), evaluated at p [m].
Vec3 loop_field(double a, double i, int axis, double axis_offset, Vec3 p, double mu0);

// A coil as a set of filamentary loops: each loop (radius, axial position) carries current i * turns_per_filament.
struct Coil {
    int axis = 0;                                // coil axis
    struct Filament { double radius, position, turns; };
    std::vector<Filament> filaments;
    Vec3 field_per_amp(Vec3 p, double mu0) const;   // [T/A]
};

// Helmholtz pair: two coils of `turns` each, mean radius `radius`, centres at +-spacing/2 on `axis`, rectangular
// winding section width x depth, discretised into nw x nd filaments per coil (legacy: 5 x 5).
Coil helmholtz_pair(int axis, double radius, double spacing, double turns, double width, double depth, int nw, int nd);

// Solenoid wound in layers: layer k has turns[k] turns at radius radii[k], pitch `pitch`, starting at x_start
// along `axis`.
Coil layered_solenoid(int axis, const std::vector<double>& radii, const std::vector<int>& turns, double pitch,
                      double x_start);

}  // namespace nmr
