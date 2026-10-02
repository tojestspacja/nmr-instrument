// Experiment descriptions -> pulse programs.
//
// An experiment is a list of elements (pulse, delay, acquire). The compiler owns receiver protection: it blanks the
// receiver pre_blank before every pulse, keeps it blanked for dead_time after, and only opens acquisition windows on
// an enabled receiver. FID, Hahn echo, inversion recovery and CPMG are element lists (see builders below); a new
// experiment is a new list, not a new scheduler.
#pragma once

#include <string>
#include <vector>

#include "pulse/program.hpp"

namespace pulse {

struct Element {
    enum Kind { Pulse, Delay, Acquire } kind = Delay;
    double duration_s = 0;      // pulse length, delay, or acquisition window length
    double phase_turns = 0;     // pulse RF phase (Pulse)
    double amplitude = 1.0;     // fraction of full scale (Pulse)
    uint32_t marker = 0;        // marker emitted at the start of a Pulse (0 = none)
    uint32_t window = 0;        // acquisition window id (Acquire)
};

struct Timing {                 // values normally from generated/instrument_config
    double tick_hz = 0;
    double tx_freq_hz = 0;
    double pre_blank_s = 0;
    double dead_time_s = 0;
    double repetition_s = 0;    // scan start to scan start
};

// A scan = element list + the receiver phase the DSP applies to its data.
struct Scan {
    std::vector<Element> elements;
    double rx_phase_turns = 0;
};

// Compile scans into one program; scan k starts at k * repetition_s. Fails with diagnostics if an element cannot be
// placed (e.g. a delay shorter than the protection margins require, a scan longer than repetition_s).
bool compile(const std::vector<Scan>& scans, const Timing& tm, Program& out, std::vector<Diagnostic>& diag);

// Phase cycles. CYCLOPS: pulse phases and receiver phase advance together by 90 degrees per scan.
enum class Cycle { None, Cyclops };

// Builders (times in seconds). Each returns the scan list for n_avg scans with the chosen phase cycle.
std::vector<Scan> fid(double t90, double acq_start, double acq_len, int n_avg, Cycle c);
std::vector<Scan> hahn_echo(double t90, double t180, double tau, double acq_len, int n_avg, Cycle c);
std::vector<Scan> inversion_recovery(double t90, double t180, double ti, double acq_start, double acq_len, int n_avg,
                                     Cycle c);
std::vector<Scan> cpmg(double t90, double t180, double tau, int n_echoes, double acq_len, int n_avg, Cycle c);

}  // namespace pulse
