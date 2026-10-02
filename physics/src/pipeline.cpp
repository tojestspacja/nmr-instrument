#include "nmr/pipeline.hpp"

#include <cmath>

namespace nmr {

Processed process(const std::vector<RecordView>& recs, const PipelineConfig& c) {
    Processed out;
    if (recs.empty()) return out;
    const double lsb = c.adc_full_scale / std::ldexp(1.0, c.adc_bits - 1);
    const double fs_out = c.adc_rate / static_cast<double>(c.decimation);
    const double fc = c.fir_cutoff_hz > 0 ? c.fir_cutoff_hz : 0.4 * fs_out;
    const dsp::FirDecimator fir(fc, c.adc_rate, c.fir_taps, c.decimation);
    out.fs = fs_out;
    size_t n_out = 0;
    std::vector<double> vi, vq, vi_d;
    std::vector<dsp::cplx> bb, dec;
    double t_rel0 = 0;
    for (size_t r = 0; r < recs.size(); ++r) {
        const RecordView& rv = recs[r];
        vi.resize(rv.n);
        vq.resize(rv.n);
        for (size_t k = 0; k < rv.n; ++k) { vi[k] = rv.i[k] * lsb; vq[k] = rv.q[k] * lsb; }
        if (c.iq_skew_samples != 0.0) {   // realign I onto Q's sampling grid before the two are combined
            vi_d.resize(rv.n);
            dsp::frac_delay(vi.data(), rv.n, c.iq_skew_samples, c.iq_fd_taps, vi_d.data());
            vi.swap(vi_d);
        }
        dsp::cplx off = 0;
        if (c.offset_tail > 0) {
            // the IF tone averages out over the tail; what remains is the DC offset of each channel
            const size_t n0 = rv.n - static_cast<size_t>(rv.n * c.offset_tail);
            double si = 0, sq = 0;
            for (size_t k = n0; k < rv.n; ++k) { si += vi[k]; sq += vq[k]; }
            off = dsp::cplx(si, sq) / static_cast<double>(rv.n - n0);
        }
        bb.resize(rv.n);
        dsp::mix_down(vi.data(), vq.data(), rv.n, off, rv.f_if, c.adc_rate, rv.t_first, bb.data());
        dec.resize(rv.n / c.decimation + 1);
        const size_t m = fir.process(bb.data(), rv.n, dec.data(), dec.size());
        dec.resize(m);
        // the receiver chain conjugates the rotating-frame signal (a line above f_tx lands above the IF), so a pulse
        // phase +phi appears as -phi in the data: undo it with +phi, and the beat phase (added at the mixer) with -beat
        const double phase = (c.correct_beat ? rv.beat_phase : 0.0) - 2.0 * 3.14159265358979323846 * rv.rx_phase_turns;
        dsp::rotate(dec.data(), m, phase);
        const double t_rel = rv.t_first - rv.t_excitation + fir.group_delay_samples() / c.adc_rate;
        if (r == 0) {
            t_rel0 = t_rel;
            n_out = m;
            out.average.assign(m, dsp::cplx(0));
        } else {
            if (std::fabs(t_rel - t_rel0) > 1e-3 / c.adc_rate) out.aligned = false;
            if (m < n_out) { n_out = m; out.average.resize(m); }
        }
        dsp::accumulate(out.average.data(), dec.data(), n_out);
        out.scans.push_back(dec);
    }
    for (auto& v : out.average) v /= static_cast<double>(recs.size());
    out.n_scans = recs.size();
    out.t0 = t_rel0;
    return out;
}

}  // namespace nmr
