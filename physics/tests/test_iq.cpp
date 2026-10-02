// FW-IQ-001: I/Q aperture-skew correction. The ADC converts the two channels one conversion period apart, so I is
// sampled ~0.5 raw samples after Q. This checks (1) the fractional-delay primitive is a true time delay for both
// frequency signs, (2) modelling the skew in the simulator and correcting it in the pipeline recovers the ideal signal
// for a line above and below f_tx, (3) the correction has the right sign, (4) the error is localised to the edges, and
// (5) the simulation is deterministic with the skew on.
#include <algorithm>
#include <cmath>
#include <vector>

#include "check.hpp"
#include "instrument_config.hpp"
#include "nmr/dsp.hpp"
#include "nmr/pipeline.hpp"
#include "nmr/sim.hpp"
#include "pulse/compiler.hpp"

using namespace nmr;
using namespace nmr::cfg;
static const double PI = 3.14159265358979323846;

static pulse::Program compile_or_die(const std::vector<pulse::Scan>& s, double f_tx) {
    pulse::Timing tm{TIMING_TICK_HZ, f_tx, TIMING_PRE_BLANK_S, RECEIVER_DEAD_TIME_S, ACQUISITION_REPETITION_TIME_S};
    pulse::Program p;
    std::vector<pulse::Diagnostic> d;
    if (!pulse::compile(s, tm, p, d)) { std::printf("compile failed: %s\n", d[0].message.c_str()); std::exit(2); }
    return p;
}

static Processed run(const SimResult& r, const InstrumentModel& m, double correct_skew) {
    std::vector<RecordView> v;
    for (const auto& x : r.records) {
        RecordView rv;
        rv.scan = x.scan; rv.window = x.window; rv.t_first = x.t_first; rv.t_excitation = x.t_excitation;
        rv.rx_phase_turns = x.rx_phase_turns; rv.beat_phase = x.beat_phase_measured; rv.f_if = x.f_tx - m.lo_hz;
        rv.i = x.i.data(); rv.q = x.q.data(); rv.n = x.i.size();
        v.push_back(rv);
    }
    PipelineConfig c;
    c.adc_rate = m.adc_rate; c.adc_full_scale = m.adc_full_scale; c.adc_bits = m.adc_bits;
    c.decimation = static_cast<size_t>(ACQUISITION_DECIMATION); c.fir_taps = 63; c.offset_tail = 0; c.correct_beat = true;
    c.iq_skew_samples = correct_skew;
    return process(v, c);
}

// relative RMS difference of two baseband averages over an interior window (fraction f0..f1 of the shorter array)
static double rel_err(const Processed& a, const Processed& b, double f0, double f1) {
    const size_t n = std::min(a.average.size(), b.average.size());
    const size_t lo = static_cast<size_t>(f0 * n), hi = static_cast<size_t>(f1 * n);
    double sig = 0, err = 0;
    for (size_t k = lo; k < hi; ++k) { sig += std::norm(a.average[k]); err += std::norm(a.average[k] - b.average[k]); }
    return sig > 0 ? std::sqrt(err / sig) : 0;
}

int main() {
    // ---- 1. frac_delay is a true time delay for +f and -f ----
    {
        const double fs = ACQUISITION_DECIMATED_RATE_HZ, d = 0.4;   // from generated config, not a hand-typed literal
        const size_t n = 400, taps = 31, guard = taps;   // skip edge transients
        for (double f : {+3000.0, -3000.0}) {
            std::vector<double> ci(n), cq(n), di(n), dq(n);
            for (size_t k = 0; k < n; ++k) { const double th = 2 * PI * f * k / fs; ci[k] = std::cos(th); cq[k] = std::sin(th); }
            dsp::frac_delay(ci.data(), n, d, taps, di.data());
            dsp::frac_delay(cq.data(), n, d, taps, dq.data());
            double worst = 0;
            for (size_t k = guard; k < n - guard; ++k) {
                // delayed complex tone should equal the original rotated by the delay's phase
                const std::complex<double> got(di[k], dq[k]);
                const std::complex<double> want = std::polar(1.0, 2 * PI * f * (k - d) / fs);
                worst = std::max(worst, std::abs(got - want));
            }
            CHECK(worst < 5e-3, f > 0 ? "frac_delay: +f tone delayed by the correct phase" :
                                        "frac_delay: -f tone delayed by the correct phase");
        }
        std::vector<double> x(n), y(n), yb(n);
        for (size_t k = 0; k < n; ++k) x[k] = std::cos(0.1 * k);
        dsp::frac_delay(x.data(), n, 0.0, taps, y.data());
        double same = 0;
        for (size_t k = 0; k < n; ++k) same = std::max(same, std::fabs(x[k] - y[k]));
        CHECK(same == 0.0, "frac_delay: delay 0 copies the input exactly");
        // edges are zero-padded, so the first/last ~taps/2 outputs are transients; they must stay bounded (no ringing)
        dsp::frac_delay(x.data(), n, 0.5, taps, yb.data());
        double mx = 0;
        for (size_t k = 0; k < n; ++k) mx = std::max(mx, std::fabs(yb[k]));
        CHECK(mx < 1.3, "frac_delay: zero-padded edges stay bounded (unit tone -> no edge blow-up)");
    }

    // ---- 2-4. end-to-end skew correction, line above and below f_tx ----
    const ProbeGeometry pg = design_probe();
    const Vec3 b0c = pg.b0.field_per_amp({0, 0, 0}, pg.mu0), b1c = pg.rf.field_per_amp({0, 0, 0}, pg.mu0);
    InstrumentModel base = design_model();
    base.noise_density = 0;
    const double t90 = TX_T90_CALCULATED_S;
    Voxel v0;
    v0.p = {0, 0, 0}; v0.volume = 1e-4; v0.b0_per_amp = b0c.norm(); v0.b0_hat = b0c * (1 / b0c.norm());
    transverse(b0c, b1c, v0.b1_perp, v0.b1_phi);
    const double f_line = base.gamma_bar * v0.b0_per_amp * base.b0_current;
    const double skew = 0.5;   // I sampled half a raw sample after Q (ADS8688 auto-sequence, Q channel below I)

    for (double offset : {+600.0, -600.0}) {   // line 600 Hz above / below f_tx -> baseband tone at -/+600 Hz
        InstrumentModel m = base;
        m.tank_f0 = f_line;
        m.t2 = 0.08;
        const auto prog = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.1, 1, pulse::Cycle::None), f_line - offset);

        InstrumentModel m_clean = m; m_clean.iq_skew_samples = 0.0;
        InstrumentModel m_skew = m;  m_skew.iq_skew_samples = skew;
        const Processed ref = run(simulate(prog, {v0}, m_clean), m, 0.0);          // ideal
        const Processed corrected = run(simulate(prog, {v0}, m_skew), m, skew);    // skewed + corrected
        const Processed uncorrected = run(simulate(prog, {v0}, m_skew), m, 0.0);   // skewed, not corrected
        const Processed wrongsign = run(simulate(prog, {v0}, m_skew), m, -skew);   // wrong-sign correction

        const double e_corr = rel_err(ref, corrected, 0.05, 0.95);
        const double e_unc = rel_err(ref, uncorrected, 0.05, 0.95);
        const double e_wrong = rel_err(ref, wrongsign, 0.05, 0.95);
        std::printf("  offset %+.0f Hz: err corrected %.2e, uncorrected %.2e, wrong-sign %.2e\n", offset, e_corr, e_unc, e_wrong);
        const char* w = offset > 0 ? "line above f_tx" : "line below f_tx";
        CHECK(e_unc > 1e-3, w);                                   // the skew is actually visible
        CHECK(e_corr < 0.15 * e_unc, w);                         // correction removes most of it
        CHECK(e_corr < 1e-2, w);                                 // and the residual is small
        CHECK(e_wrong > e_unc, w);                                // wrong sign is worse than doing nothing
    }

    // ---- 5. transient boundaries: including the edges, the corrected error stays small (edges do not destabilise) ----
    {
        InstrumentModel m = base; m.tank_f0 = f_line; m.t2 = 0.08;
        InstrumentModel m_clean = m; m_clean.iq_skew_samples = 0.0;
        InstrumentModel m_skew = m;  m_skew.iq_skew_samples = skew;
        const auto prog = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.1, 1, pulse::Cycle::None), f_line - 600.0);
        const Processed ref = run(simulate(prog, {v0}, m_clean), m, 0.0);
        const Processed corrected = run(simulate(prog, {v0}, m_skew), m, skew);
        const double e_all = rel_err(ref, corrected, 0.0, 1.0);
        CHECK(e_all < 1e-2, "correction stays bounded over the whole record including the edges (transient boundaries)");
    }

    // ---- 6. determinism: the skewed simulation replays bit-for-bit ----
    {
        InstrumentModel m = base; m.tank_f0 = f_line; m.iq_skew_samples = skew; m.seed = 7; m.noise_density = 1e-8;
        const auto prog = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.05, 1, pulse::Cycle::None), f_line - 300.0);
        const SimResult a = simulate(prog, {v0}, m);
        const SimResult b = simulate(prog, {v0}, m);
        bool identical = a.records.size() == b.records.size() && !a.records.empty() && a.records[0].i == b.records[0].i &&
                         a.records[0].q == b.records[0].q;
        CHECK(identical, "skewed simulation is deterministic (same seed -> identical codes)");
    }

    return finish("test_iq");
}
