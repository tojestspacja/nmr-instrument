// The digital twin: executes a compiled pulse program (the same one the firmware runs) on the voxel model and
// produces what the ADC would record: I and Q codes on the free-running ADC sample grid, per acquisition window,
// with the per-scan metadata the hardware will timestamp (excitation marker time, receiver phase, TX-LO beat phase).
//
// Model chain (docs/physics.md has the equations and their sources):
//   voxel magnetization  -> exact pulse rotations / free precession with T1,T2 (bloch.hpp)
//   reciprocity emf      -> emf_v = omega_v * B1perp_v/I * M0_v * dV   [V per unit transverse M/M0]
//   tank                 -> H(f) = 1 / (1 - (f/f0)^2 + j (f/f0)/Q)   (|H(f0)| = Q)
//   receiver             -> real gain G, single-pole IF response, quadrature mix to the IF with the TX-LO beat phase
//   ADC                  -> + Gaussian noise (density at the tank node x G, noise bandwidth pi/2 x IF pole), offsets,
//                           16-bit quantisation and clipping at +-full scale
#pragma once

#include <cstdint>
#include <vector>

#include "nmr/probe.hpp"
#include "pulse/program.hpp"

namespace nmr {

struct InstrumentModel {
    // physics
    double gamma_bar = 0, b0_current = 0, t1 = 0, t2 = 0, proton_density = 0, temperature = 0, hbar = 0, k_b = 0;
    Vec3 earth_field{};                 // [T] added to the B0 pair's field
    // transmit
    double tx_coil_current = 0;         // [A pk] in the RF coil at full amplitude
    // receive
    double tank_f0 = 0, tank_q = 0;     // [Hz], [-]
    double rx_gain = 0;                 // tank node -> ADC input, real [V/V] (LNA x mixer x IF)
    double if_pole_hz = 0;
    double noise_density = 0;           // [V/sqrt(Hz)] at the tank node
    double lo_hz = 0;
    double adc_full_scale = 0;          // [V] (+-)
    int adc_bits = 16;
    double adc_rate = 0;                // raw samples/s per channel
    double adc_t0 = 0;                  // phase of the free-running ADC grid [s]
    double offset_i = 0, offset_q = 0;  // [V] at the ADC
    double iq_skew_samples = 0;         // I converted this many raw samples after Q (0 = ideal; hardware ~0.5, FW-IQ-001)
    // coherence (ADR-0006)
    bool lo_coherent = true;            // false: each scan starts at a random TX-LO beat phase
    double beat_measurement_sigma = 0;  // [rad] error of the per-scan beat-phase measurement (timestamps)
    uint64_t seed = 1;
    // numerics
    int interp = 8;                     // fast path: evaluate the baseband every `interp` ADC samples (1 = reference)
};

InstrumentModel design_model();         // from generated/instrument_config.hpp

struct Record {
    uint32_t scan = 0, window = 0;
    double t_first = 0;                 // time of the first sample [s from program start]
    double t_excitation = 0;            // start of this scan's excitation pulse (marker 1) [s]
    double rx_phase_turns = 0;          // from RX_PHASE
    double beat_phase_true = 0;         // [rad] (simulation only)
    double beat_phase_measured = 0;     // [rad] what the hardware timestamps report
    double f_tx = 0;                    // from SET_FREQ [Hz]
    std::vector<int16_t> i, q;          // ADC codes
};

struct SimResult {
    std::vector<Record> records;
    double emf_peak = 0;                // largest |emf| sum seen [V] (diagnostic)
    double adc_clip_fraction = 0;
};

SimResult simulate(const pulse::Program& prog, const std::vector<Voxel>& vox, const InstrumentModel& m);

// convert codes back to volts
inline double code_to_volts(int16_t c, const InstrumentModel& m) { return c * m.adc_full_scale / 32768.0; }

}  // namespace nmr
