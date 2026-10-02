// The digital twin end to end: program -> voxels -> ADC codes -> pipeline -> spectrum.
#include <algorithm>
#include <cmath>

#include "check.hpp"
#include "instrument_config.hpp"
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

static Processed run_pipeline(const SimResult& r, const InstrumentModel& m, bool correct_beat, uint32_t window = 0) {
    std::vector<RecordView> v;
    for (const auto& x : r.records) {
        if (x.window != window) continue;
        RecordView rv;
        rv.scan = x.scan; rv.window = x.window; rv.t_first = x.t_first; rv.t_excitation = x.t_excitation;
        rv.rx_phase_turns = x.rx_phase_turns; rv.beat_phase = x.beat_phase_measured; rv.f_if = x.f_tx - m.lo_hz;
        rv.i = x.i.data(); rv.q = x.q.data(); rv.n = x.i.size();
        v.push_back(rv);
    }
    PipelineConfig c;
    c.adc_rate = m.adc_rate; c.adc_full_scale = m.adc_full_scale; c.adc_bits = m.adc_bits;
    c.decimation = static_cast<size_t>(ACQUISITION_DECIMATION); c.fir_taps = 63; c.offset_tail = 0; c.correct_beat = correct_beat;
    return process(v, c);
}

int main() {
    const ProbeGeometry pg = design_probe();
    const Vec3 b0c = pg.b0.field_per_amp({0, 0, 0}, pg.mu0), b1c = pg.rf.field_per_amp({0, 0, 0}, pg.mu0);
    InstrumentModel m = design_model();
    m.noise_density = 0;
    const double t90 = TX_T90_CALCULATED_S;   // exact pi/2 at the coil centre

    // ---- one voxel at the centre, 100 mL: amplitude, frequency sign, T2 decay
    Voxel v0;
    v0.p = {0, 0, 0}; v0.volume = 1e-4; v0.b0_per_amp = b0c.norm(); v0.b0_hat = b0c * (1 / b0c.norm());
    transverse(b0c, b1c, v0.b1_perp, v0.b1_phi);
    const double f_line = m.gamma_bar * v0.b0_per_amp * m.b0_current;
    {
        const double offset = 37.0;            // transmit 37 Hz below the line
        InstrumentModel mm = m;
        mm.tank_f0 = f_line;
        mm.t2 = 0.05;
        const auto prog = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.2, 1, pulse::Cycle::None), f_line - offset);
        const SimResult r = simulate(prog, {v0}, mm);
        CHECK(r.records.size() == 1, "one record");
        const Processed p = run_pipeline(r, mm, true);
        // expected amplitude at the ADC, from the model equations (docs/physics.md)
        const double m0 = curie_magnetization(mm.proton_density, mm.gamma_bar, v0.b0_per_amp * mm.b0_current, mm.temperature, mm.hbar, mm.k_b);
        const double emf = 2 * PI * f_line * v0.b1_perp * m0 * v0.volume;
        const double rr = f_line / mm.tank_f0;
        const double tank = 1.0 / std::abs(std::complex<double>(1 - rr * rr, rr / mm.tank_q));
        const double ifr = 1.0 / std::abs(std::complex<double>(1, (f_line - mm.lo_hz) / mm.if_pole_hz));
        const double expect = emf * tank * ifr * mm.rx_gain;
        // the flip is exactly 90 degrees only on resonance; at 37 Hz offset the transverse magnitude is ~1 - O(1e-3)
        const double a0 = std::abs(p.average[0]) * std::exp(p.t0 / mm.t2);
        CHECK_NEAR(a0, expect, 0, 0.01, "single-voxel FID amplitude at the ADC [V] (model equation)");
        const double a1 = std::abs(p.average[2000]) * std::exp((p.t0 + 2000 / p.fs) / mm.t2);
        CHECK_NEAR(a1, a0, 0, 0.005, "T2 decay: amplitude * exp(t/T2) constant");
        const auto s = dsp::spectrum(p.average.data(), p.average.size(), p.fs, dsp::Window::Rect, 0, 8);
        const auto pk = dsp::find_peak(s, -200, 200, 10);
        CHECK_NEAR(pk.freq_hz, offset, 0.2, 0, "line 37 Hz above f_tx appears at +37 Hz (I/Q sign convention)");
        CHECK(r.adc_clip_fraction == 0, "no ADC clipping");
    }
    // ---- CYCLOPS averages coherently; an unsynchronised LO destroys the average; the measured beat phase restores it
    {
        InstrumentModel mm = m;
        mm.tank_f0 = f_line;
        mm.t1 = 0.1;   // full recovery between scans (TR >> T1), so every scan starts from equilibrium
        mm.t2 = 0.1;
        const auto one = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.1, 1, pulse::Cycle::None), f_line);
        const auto eight = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.1, 8, pulse::Cycle::Cyclops), f_line);
        const double a1 = std::abs(run_pipeline(simulate(one, {v0}, mm), mm, true).average[10]);
        const double a8 = std::abs(run_pipeline(simulate(eight, {v0}, mm), mm, true).average[10]);
        CHECK_NEAR(a8, a1, 0, 0.005, "CYCLOPS x8 average = single scan (coherent LO)");
        mm.lo_coherent = false;
        mm.seed = 11;
        const SimResult inc = simulate(eight, {v0}, mm);
        const double raw = std::abs(run_pipeline(inc, mm, false).average[10]);
        const double fixed = std::abs(run_pipeline(inc, mm, true).average[10]);
        CHECK(raw < 0.7 * a1, "random TX-LO beat phase per scan: uncorrected average collapses");
        CHECK_NEAR(fixed, a1, 0, 0.005, "measured beat phase restores the coherent average");
        mm.beat_measurement_sigma = 0.1;   // 5.7 degrees rms measurement error
        const double noisy = std::abs(run_pipeline(simulate(eight, {v0}, mm), mm, true).average[10]);
        CHECK_NEAR(noisy, a1 * std::exp(-0.005), 0, 0.01, "beat measurement error sigma costs exp(-sigma^2/2)");
    }
    // ---- progressive saturation: TR comparable to T1. Scan 1 starts at Mz = 1, later scans at 1 - exp(-TR'/T1)
    //      (TR' = TR - t90; T2 << TR so no transverse remainder). Average of 8 = (1 + 7 (1 - E1)) / 8.
    {
        InstrumentModel mm = m;
        mm.tank_f0 = f_line;
        mm.t2 = 0.05;
        const auto one = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.1, 1, pulse::Cycle::None), f_line);
        const auto eight = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.1, 8, pulse::Cycle::Cyclops), f_line);
        const double a1 = std::abs(run_pipeline(simulate(one, {v0}, mm), mm, true).average[10]);
        const double a8 = std::abs(run_pipeline(simulate(eight, {v0}, mm), mm, true).average[10]);
        const double e1 = std::exp(-(ACQUISITION_REPETITION_TIME_S - t90) / mm.t1);
        CHECK_NEAR(a8 / a1, (1 + 7 * (1 - e1)) / 8, 0, 2e-3, "progressive saturation: average of 8 scans at TR ~ T1");
    }
    // ---- design sample: reference path vs fast path, line position, noise level
    {
        const std::vector<Voxel> vox = voxelize(pg, design_sample(), 0.005);
        InstrumentModel mm = m;
        const auto prog = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.05, 1, pulse::Cycle::None), mm.tank_f0);
        mm.interp = 1;
        const SimResult ref = simulate(prog, vox, mm);
        mm.interp = 8;
        const SimResult fast = simulate(prog, vox, mm);
        int worst = 0;
        for (size_t k = 0; k < ref.records[0].i.size(); ++k)
            worst = std::max({worst, std::abs(ref.records[0].i[k] - fast.records[0].i[k]), std::abs(ref.records[0].q[k] - fast.records[0].q[k])});
        CHECK(worst <= 2, "fast path (interp 8) within 2 LSB of the reference path");
        std::printf("  fast vs reference: worst difference %d LSB\n", worst);
        // line: the voxels' Larmor frequencies relative to f_tx, weighted median vs the spectrum peak
        const Processed p = run_pipeline(fast, mm, true);
        const auto s = dsp::spectrum(p.average.data(), p.average.size(), p.fs, dsp::Window::Rect, 0, 8);
        const auto pk = dsp::find_peak(s, -400, 400, 10);
        CHECK_NEAR(pk.freq_hz, m.gamma_bar * b0c.norm() * m.b0_current - mm.tank_f0, 25, 0, "design-sample line near the centre Larmor offset");
        std::printf("  design sample: line at %+.1f Hz from f_tx (centre voxel %+.1f Hz)\n", pk.freq_hz, m.gamma_bar * b0c.norm() * m.b0_current - mm.tank_f0);
    }
    {
        InstrumentModel mm = design_model();
        mm.proton_density = 0;   // no signal: noise only
        const auto prog = compile_or_die(pulse::fid(t90, ACQUISITION_START_DELAY_S, 0.2, 1, pulse::Cycle::None), mm.tank_f0);
        const SimResult r = simulate(prog, {v0}, mm);
        double s2 = 0;
        for (auto c : r.records[0].i) s2 += code_to_volts(c, mm) * code_to_volts(c, mm);
        const double sd = std::sqrt(s2 / r.records[0].i.size());
        const double expect = mm.noise_density * mm.rx_gain * std::sqrt(0.5 * PI * mm.if_pole_hz);
        CHECK_NEAR(sd, expect, 0, 0.03, "ADC noise rms = e_n G sqrt(pi/2 f_IF-pole)");
    }
    // ---- Hahn echo on the design sample: refocusing at 2 tau
    {
        const std::vector<Voxel> vox = voxelize(pg, design_sample(), 0.005);
        InstrumentModel mm = m;
        const double tau = 0.020;
        const auto prog = compile_or_die(pulse::hahn_echo(t90, 2 * t90, tau, 0.030, 1, pulse::Cycle::None), mm.tank_f0);
        const Processed p = run_pipeline(simulate(prog, vox, mm), mm, true);
        size_t kmax = 0;
        for (size_t k = 0; k < p.average.size(); ++k)
            if (std::abs(p.average[k]) > std::abs(p.average[kmax])) kmax = k;
        const double t_echo = p.t0 + kmax / p.fs;   // from the excitation start
        CHECK_NEAR(t_echo, 2 * tau + t90 / 2, 1.0e-3, 0, "echo maximum at 2 tau after the 90-degree pulse centre");
        std::printf("  echo maximum %.3f ms after excitation start\n", t_echo * 1e3);
    }
    return finish("test_sim");
}
