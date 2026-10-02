// The receive-processing chain, one implementation for firmware, host tools and the browser:
//   ADC codes -> volts -> DC offset removal -> complex IF (I + jQ) -> digital mix to baseband (absolute time, so the
//   TX-LO beat is accounted for) -> FIR low-pass + decimation -> per-scan phase correction (receiver phase of the
//   phase cycle + measured beat phase, ADR-0006) -> coherent average -> (window -> FFT -> peak/SNR: dsp.hpp)
#pragma once

#include <cstdint>
#include <vector>

#include "nmr/dsp.hpp"

namespace nmr {

struct RecordView {               // what the hardware (or the simulator) delivers per acquisition window
    uint32_t scan = 0, window = 0;
    double t_first = 0;           // [s] time of the first sample, program clock
    double t_excitation = 0;      // [s] excitation marker time, same clock
    double rx_phase_turns = 0;    // phase-cycle receiver phase
    double beat_phase = 0;        // [rad] measured TX-LO beat phase at the scan start (0 if the hardware is coherent)
    double f_if = 0;              // [Hz] f_tx - f_lo for this record
    const int16_t* i = nullptr;
    const int16_t* q = nullptr;
    size_t n = 0;
};

struct PipelineConfig {
    double adc_rate = 0, adc_full_scale = 0;
    int adc_bits = 16;
    size_t decimation = 4, fir_taps = 63;
    double fir_cutoff_hz = 0;     // 0: 0.4 x output rate
    double offset_tail = 0.1;     // DC offset from the last fraction of each record (0 = none)
    bool correct_beat = true;     // apply the measured beat phase
};

struct Processed {
    double fs = 0;                // output sample rate [Hz]
    double t0 = 0;                // time of output sample 0 after the excitation marker [s]
    std::vector<dsp::cplx> average;
    std::vector<std::vector<dsp::cplx>> scans;   // per scan, after phase correction (for diagnostics)
    size_t n_scans = 0;
    bool aligned = true;          // all scans had the same sample timing relative to excitation
};

Processed process(const std::vector<RecordView>& recs, const PipelineConfig& c);

}  // namespace nmr
