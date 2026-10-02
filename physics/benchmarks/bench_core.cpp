// Benchmarks of the representative workloads (docs/physics.md#performance). Prints median of repeated runs.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <vector>

#include "instrument_config.hpp"
#include "nmr/dsp.hpp"
#include "nmr/pipeline.hpp"
#include "nmr/sim.hpp"
#include "pulse/compiler.hpp"

using namespace nmr;
using namespace nmr::cfg;

static double time_ms(const std::function<void()>& f, int reps) {
    std::vector<double> t;
    for (int i = 0; i < reps; ++i) {
        const auto a = std::chrono::steady_clock::now();
        f();
        t.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count());
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

int main() {
    std::vector<Voxel> vox;
    const double tv = time_ms([&] { vox = voxelize(design_probe(), design_sample(), 0.0025); }, 3);
    std::printf("voxelize 2.5 mm grid: %zu voxels, %.1f ms\n", vox.size(), tv);

    pulse::Timing tm{TIMING_TICK_HZ, TX_FREQUENCY_HZ, TIMING_PRE_BLANK_S, RECEIVER_DEAD_TIME_S, ACQUISITION_REPETITION_TIME_S};
    pulse::Program prog;
    std::vector<pulse::Diagnostic> d;
    const double tc = time_ms([&] {
        pulse::compile(pulse::hahn_echo(TX_T90_S, TX_T180_S, 0.02, 0.02, static_cast<int>(ACQUISITION_AVERAGES), pulse::Cycle::Cyclops), tm, prog, d);
    }, 50);
    std::printf("compile Hahn echo x%d: %zu instructions, %.3f ms\n", static_cast<int>(ACQUISITION_AVERAGES), prog.code.size(), tc);
    const double tcp = time_ms([&] { pulse::compile(pulse::cpmg(TX_T90_S, TX_T180_S, 0.01, 64, 0.005, 4, pulse::Cycle::Cyclops), tm, prog, d); }, 20);
    std::printf("compile CPMG 64 echoes x4: %zu instructions, %.3f ms\n", prog.code.size(), tcp);

    pulse::compile(pulse::fid(TX_T90_S, ACQUISITION_START_DELAY_S, ACQUISITION_DURATION_S, 1, pulse::Cycle::None), tm, prog, d);
    InstrumentModel m = design_model();
    SimResult r;
    for (int k : {1, 8}) {
        m.interp = k;
        const double ts = time_ms([&] { r = simulate(prog, vox, m); }, 3);
        std::printf("simulate one FID scan (%zu voxels, %.2f s, %zu ADC samples/ch), interp %d: %.1f ms\n", vox.size(),
                    ACQUISITION_DURATION_S, r.records[0].i.size(), k, ts);
    }
    std::vector<RecordView> v(1);
    v[0] = {0, 0, r.records[0].t_first, r.records[0].t_excitation, 0, 0, r.records[0].f_tx - m.lo_hz,
            r.records[0].i.data(), r.records[0].q.data(), r.records[0].i.size()};
    PipelineConfig pc;
    pc.adc_rate = m.adc_rate; pc.adc_full_scale = m.adc_full_scale; pc.decimation = 4;
    Processed p;
    const double tp = time_ms([&] { p = process(v, pc); }, 10);
    std::printf("pipeline one record (%zu -> %zu samples): %.2f ms\n", v[0].n, p.average.size(), tp);
    for (size_t n : {4096u, 65536u}) {
        std::vector<dsp::cplx> x(n, 1.0);
        dsp::FftPlan plan(n);
        const double tf = time_ms([&] { plan.execute(x.data()); }, 20);
        std::printf("FFT %zu points: %.3f ms\n", n, tf);
    }
    std::vector<dsp::cplx> x(4096, 1.0), y(4096);
    const double td = time_ms([&] { dsp::dft_reference(x.data(), y.data(), 4096); }, 3);
    std::printf("naive DFT 4096 points (legacy approach): %.1f ms\n", td);
    return 0;
}
